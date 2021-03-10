/* ================= Import libraries START AVISH================= */

#include <littlefs.h>         // Although littlefs is inported so FS is not required but , in my case I had to add FS
#include <Ticker.h>           //for LED status if Wifi
#include <Ticker2.h>          //For rgbled
#include <WiFiManager.h>      //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <DNSServer.h>        //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h> //Local WebServer used to serve the configuration portal

// <ESP> + <OTA>
#include <ESP8266WiFi.h>       // For basic functionality of ESP8266
#include <ESP8266HTTPClient.h> // For HTTP client functionality of ESP8266
#include <ESP8266httpUpdate.h> // For OTA functionality of ESP8266

#include <CertStoreBearSSL.h>
BearSSL::CertStore certStore;

#include <ESP8266HTTPUpdateServer.h>

// Fast led for RGB leds
#include <FastLED.h>

//mDNS
#include <ESP8266mDNS.h> // Include the mDNS library

// <AWS IOT + MQTT - PUB/SUB>

#include <PubSubClient.h> // For ESP8266 to publish/subscribe messaging with aws MQTT server
#include <NTPClient.h>    // To connect Network Time Protocol (NTP) server for epoch time
#include <WiFiUdp.h>      // Implementations send and receive timestamps using the User Datagram Protocol (UDP)

#include <WebSocketsServer.h>

#include <CertStoreBearSSL.h>

#include <ESP8266HTTPUpdateServer.h>

#include <ArduinoJson.h> // Serial data <-> JSON data , for conversions

// <For PWM led task queue>
#include <cppQueue.h> // For PWM led task queue

#include "amorlamps.h"

// HTTP_HEAD

/* ================= Import libraries END ================= */

const char *fsName = "LittleFS";
FS *fileSystem = &LittleFS;
LittleFSConfig fileSystemConfig = LittleFSConfig();

static bool fsOK;

//used in removeFromConfigJSON
bool isToDeleteupdatetoConfigJSONflag = false;

// # define Serial.printf "Serial.println"
const String FirmwareVer = {"2.03"};

// #define DEBUG_AMOR 1 // TODO:comment in productions

// <Interrupts>
//-common-                                            // Volatile because it is changed by ISR ,
const uint16_t debounceDuration = 300; // Debounce duration for any interrupt
unsigned long currrentMillis_interrupt = 0;

// Interrupts For built-in-button
const uint8_t builtInButton = 0;            // On board button of Node MCU 1.0
unsigned long lastValidInterruptTime_1 = 0; // When was the last valid millis on which board push button was pressed
volatile bool myISR1_flag = 0;              // ISR flag for on board button press

// Interrupts For Touch sensor input
const uint8_t touchInButton = 15;           // Touch sensor GPIO input pin
unsigned long lastValidInterruptTime_2 = 0; // When was the last valid millis on which lamp Touched
volatile bool myISR2_flag = 0;              // ISR flag for touch interrupt

uint8_t myISR2_flag_counter = 0; // TODO: what to do about it ?
uint8_t myISR2_flag_counter_cooldown = 0;
unsigned long myISR2_flag_counter_cooldown_millis = 0;

#define FASTLED_ESP8266_NODEMCU_PIN_ORDER
// How many leds in your strip?
#define NUM_LEDS 12 //TODO: check no.s of strips
// RGB led
#define DATA_PIN 4 //d2
// Define the array of leds
CRGB leds[NUM_LEDS];

// bool updateDeviceShadow_toSendHSL_flag = false;

uint8_t my_rgb_hsv_values[3] = {65, 255, 0};
uint8_t tosend_rgb_hsv_values[3] = {100, 255, 0};
bool update_shadow_tosend_rgb_hsv_flag = false;
unsigned long update_shadow_tosend_rgb_hsv_last_millis = 0;
unsigned int update_shadow_tosend_rgb_hsv_interval = 10000;
uint8_t current_rgb_hsv_values[3] = {0, 0, 0};
uint8_t desired_rgb_hsv_values[3] = {0, 0, 0};

unsigned long touchEpoch = 0;
int turn_on_RGB_led_for_x_mins_counter = 0;
float turn_on_RGB_led_for_x_mins_intensity_delta = 0;
int x_min_on_value = 20;

bool isToSend_flag = false; // isToSend_flag tells which color to fade in out , incomming or outgoing
int fade_in_out_RGB_x_times_counter = 0;
uint16_t fade_in_out_RGB_x_times_temp_intensity = 0;
uint16_t fade_in_out_RGB_x_times_temp_intensity_HOLD = 0;

int blink_led_x_times_counter = 0;
bool blink_led_x_times_toSendHsv = true;

bool rgb_led_is_busy_flag = 0;

// enum methodCode
// {
//   MSKIP,
//   MSETHSV,     // 1: set_single_RGB_color
//   MFADEINOUT,  // 2: fade_in_out_RGB_x_times
//   MXMINSON,    // 3: turn_on_RGB_led_for_x_mins
//   MUMYRGB,     // 4: update_my_rgb_hsv
//   MUTOSENDRGB, // 5: update_tosend_rgb_hsv
//   MIRS1,       // 6: myIRS1_method();
//   MIRS2,       // 7: myIRS2_method();
//   MBLEDX       // 8: blink_led_x_times

// };

typedef struct RGBQueueTask
{
  methodCode rgbLedMethodCode; // which method to call
  int argument1;               // argument of that method

  uint8_t s; // for set rgb color HSV(h is argument 1) , default 0
  uint8_t v;

  RGBQueueTask(methodCode c, int arg) // Constructor
  {
    rgbLedMethodCode = c;
    argument1 = arg;
  }

  // for code 1 set_single_RGB_color
  RGBQueueTask(methodCode c, int arg, uint8_t sa, uint8_t va) // Constructor
  {
    rgbLedMethodCode = c;
    argument1 = arg;
    s = sa;
    v = va;
  }
};

// methodcode : method
// 0: skip ()
// 1: set_single_RGB_color
// 2: tick_fade_in_out_RGB_x_times
// 3: turn_on_RGB_led_for_x_mins

cppQueue rgb_led_task_queue(sizeof(RGBQueueTask), 5, FIFO);

// WIFI MANAGER
// WiFiManager wifiManager; //Soft wifi configuration

const uint8_t wifiManagerLED = 2; // TODO: Upadte as per the requirements

// Ticker tickerWifiManagerLed(tickWifiManagerLed, 1000, 0, MILLIS);
Ticker tickerWifiManagerLed;

// Not required as of now
// const char *http_username = "admin";
// const char *http_password = "admin";

//flag to use from web update to reboot the ESP
// bool shouldReboot = false;

String deviceId = "amorAAA_C7ED21";
String groupId = "123456";

// AWS , MQTT , PUB/SUB + internet Time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
bool setX509TimeFlag = false;

unsigned long timeClient_counter_lastvalid_millis = 0;
// uint8_t timeClient_counter = 0;

// const char *AWS_endpoint = "a2q0zes51fm6yg-ats.iot.us-west-2.amazonaws.com"; //MQTT broker ip
char *AWS_endpoint = "a3an4l5rg1sm5p-ats.iot.ap-south-1.amazonaws.com"; //MQTT broker ip
uint8_t failed_aws_trials_counter_base = 5;                             // to be updated if need to do ota , to disable restart in reconnet to aws loop
uint8_t failed_aws_trials_counter = 0;
uint8_t recon_aws_count = 0;
unsigned long reconnect_aws_millis = 0;

// const char *aws_topic = "$aws/things/esp8266_C7ED21/";

// aws_topic;

String aws_topic_str = "$aws/things/" + deviceId + "/";
// char *aws_topic = "$aws/things/esp8266_C7ED21/";
String aws_group_topic_str = "amorgroup/" + groupId + "/";

WiFiClientSecure espClient;

PubSubClient clientPubSub(AWS_endpoint, 8883, aws_callback, espClient); //set MQTT port number to 8883 as per //standard

// WebSocketsServer
ESP8266WebServer server;
WebSocketsServer webSocket = WebSocketsServer(81);
File uploadFile;

// char webpage[] PROGMEM = R"=====(ok amor)====="; // diabled after v2.0
char webpageOTA[] PROGMEM = R"=====(<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>)=====";

// ESP8266HTTPUpdateServer httpUpdater;

// BearSSL::CertStore certStore;

Ticker2 ticker_turn_on_disco_mode_for_x_mins(tick_turn_on_disco_mode_for_x_mins, 10, 0, MILLIS);

void tick_turn_on_disco_mode_for_x_mins()
{

  static uint8_t hue = 0;
  static uint8_t i = 0;
  if (i == NUM_LEDS / 2)
  {
    i = 0;
  }
  hue++;
  for (uint8_t j = 0; j < NUM_LEDS; j++)
  {
    /* code */
    leds[j].setHSV((uint8_t)(hue - (j * (13))), 255, 255);
  }

  FastLED.show();
}

void turn_on_disco_mode_for_x_mins(int x)
{
  disable_touch_for_x_ms(4000);

  //this will turn off on going timer.
  turn_off_rgb();
  ticker_turn_on_disco_mode_for_x_mins.start();
}

void turn_off_rgb()
{
  disable_touch_for_x_ms(3000);
  method_handler(MXMINSON, 0, false, 0, 0);
}

void turn_off_disco_mode()
{
  disable_touch_for_x_ms(3000);
  ticker_turn_on_disco_mode_for_x_mins.stop();
}

Ticker2 ticker_turn_on_RGB_led_for_x_mins(tick_turn_on_RGB_led_for_x_mins, 1 * 1000, 0, MILLIS);

void tick_turn_on_RGB_led_for_x_mins()
{

  turn_on_RGB_led_for_x_mins_counter--;
#ifdef DEBUG_AMOR
  Serial.println(turn_on_RGB_led_for_x_mins_counter);
#endif

  int intensity = turn_on_RGB_led_for_x_mins_counter * turn_on_RGB_led_for_x_mins_intensity_delta;
  if (intensity <= 0)
  {
    intensity = 0;
  }
  set_single_RGB_color(my_rgb_hsv_values[0], my_rgb_hsv_values[1], (uint8_t)intensity);

  if (turn_on_RGB_led_for_x_mins_counter <= 0)
  {
    ticker_turn_on_RGB_led_for_x_mins.stop();
    disable_touch_for_x_ms(200);

    set_single_RGB_color(my_rgb_hsv_values[0], my_rgb_hsv_values[1], 0);

#ifdef DEBUG_AMOR
    Serial.println(F("==ticker_turn_on_RGB_led_for_x_mins.stop();=="));
#endif
  }
}

Ticker2 ticker_set_single_RGB_color(tick_set_single_RGB_color, 100, 0, MICROS_MICROS);

void tick_set_single_RGB_color()
{

  if (current_rgb_hsv_values[0] > desired_rgb_hsv_values[0])
  {
    current_rgb_hsv_values[0]--;
  }
  else if (current_rgb_hsv_values[0] < desired_rgb_hsv_values[0])
  {
    current_rgb_hsv_values[0]++;
  }

  if (current_rgb_hsv_values[1] > desired_rgb_hsv_values[1])
  {
    current_rgb_hsv_values[1]--;
  }
  else if (current_rgb_hsv_values[1] < desired_rgb_hsv_values[1])
  {
    current_rgb_hsv_values[1]++;
  }

  if (current_rgb_hsv_values[2] > desired_rgb_hsv_values[2])
  {
    current_rgb_hsv_values[2]--;
  }
  else if (current_rgb_hsv_values[2] < desired_rgb_hsv_values[2])
  {
    current_rgb_hsv_values[2]++;
  }

  for (int k = 0; k < NUM_LEDS; k++)
  {
    leds[k].setHSV(current_rgb_hsv_values[0], current_rgb_hsv_values[1], current_rgb_hsv_values[2]);
  }

  FastLED.show();

  if (current_rgb_hsv_values[0] == desired_rgb_hsv_values[0] &&
      current_rgb_hsv_values[1] == desired_rgb_hsv_values[1] &&
      current_rgb_hsv_values[2] == desired_rgb_hsv_values[2])
  {
    ticker_set_single_RGB_color.stop();
    // disable_touch_for_x_ms(200);
    // rgb_led_is_busy_flag = 0;
    // #ifdef DEBUG_AMOR
    //     Serial.println(F("==ticker_set_single_RGB_color.stop();=="));
    // #endif
  }
}

void set_single_RGB_color(uint8_t h, uint8_t s, uint8_t v)
{

  // current_rgb_hsv_values[0] = h;
  // current_rgb_hsv_values[1] = s;
  // current_rgb_hsv_values[2] = v;

  desired_rgb_hsv_values[0] = h;
  desired_rgb_hsv_values[1] = s;
  desired_rgb_hsv_values[2] = v;

  ticker_set_single_RGB_color.start();
  // rgb_led_is_busy_flag = 1;

  // #ifdef DEBUG_AMOR
  //   Serial.println(F("==setting color t HSV =="));
  //   Serial.println(h);
  //   Serial.println(s);
  //   Serial.println(v);
  // #endif
}

void turn_on_RGB_led_for_x_mins(int x)
{

  if (ticker_turn_on_RGB_led_for_x_mins.state() == RUNNING)
  {
    ticker_turn_on_RGB_led_for_x_mins.stop();
    disable_touch_for_x_ms(200);
  }

  set_single_RGB_color(my_rgb_hsv_values[0], my_rgb_hsv_values[1], 255);

  if (x != 0)
  {
    turn_on_RGB_led_for_x_mins_intensity_delta = 255.0 / (x * 60.0);
  }
  else
  {
    turn_on_RGB_led_for_x_mins_intensity_delta = 255;
  }

  turn_on_RGB_led_for_x_mins_counter = 60 * x;

  ticker_turn_on_RGB_led_for_x_mins.start();

#ifdef DEBUG_AMOR
  Serial.println(F("==turn_on_RGB_led_for_x_mins_intensity_delta=="));
  Serial.println(turn_on_RGB_led_for_x_mins_intensity_delta);
  Serial.println(F("==turn_on_RGB_led_for_x_mins(int x)=="));
#endif
}

Ticker2 ticker_fade_in_out_RGB_x_times(tick_fade_in_out_RGB_x_times, 3, 0, MILLIS);

void tick_fade_in_out_RGB_x_times()
{
  if (isToSend_flag)
  {
    set_single_RGB_color(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], fade_in_out_RGB_x_times_temp_intensity);
  }
  else
  {
    set_single_RGB_color(my_rgb_hsv_values[0], my_rgb_hsv_values[1], fade_in_out_RGB_x_times_temp_intensity);
  }

  fade_in_out_RGB_x_times_temp_intensity = fade_in_out_RGB_x_times_temp_intensity - 1;
  fade_in_out_RGB_x_times_counter--;

  if (fade_in_out_RGB_x_times_counter <= 0)
  {
    if (isToSend_flag)
    {
      set_single_RGB_color(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], fade_in_out_RGB_x_times_temp_intensity_HOLD);
    }
    else
    {
      set_single_RGB_color(my_rgb_hsv_values[0], my_rgb_hsv_values[1], fade_in_out_RGB_x_times_temp_intensity_HOLD);
    }
    ticker_fade_in_out_RGB_x_times.stop();
    disable_touch_for_x_ms(200);
    rgb_led_is_busy_flag = 0;

#ifdef DEBUG_AMOR
    Serial.println(F("==ticker_fade_in_out_RGB_x_times.stop() =="));
    Serial.println(current_rgb_hsv_values[2]);
    Serial.println(desired_rgb_hsv_values[2]);
    Serial.println(fade_in_out_RGB_x_times_temp_intensity_HOLD);
#endif
  }

  // #ifdef DEBUG_AMOR
  //   Serial.println(F("==fade_in_out_RGB_x_times_temp_intensity=="));
  // #endif
}

void fade_in_out_RGB_x_times(int x, bool isToSend)
{
  isToSend_flag = isToSend;
  fade_in_out_RGB_x_times_counter = x * 256 * 1;
  fade_in_out_RGB_x_times_temp_intensity = desired_rgb_hsv_values[2];
  fade_in_out_RGB_x_times_temp_intensity_HOLD = fade_in_out_RGB_x_times_temp_intensity;

  disable_touch_for_x_ms(2000 * x);
  ticker_fade_in_out_RGB_x_times.start();
  rgb_led_is_busy_flag = true;
#ifdef DEBUG_AMOR
  Serial.println(F("==fade_in_out_RGB_x_times_temp_intensity_HOLD =="));
  Serial.println(fade_in_out_RGB_x_times_temp_intensity_HOLD);
#endif
}

Ticker2 ticker_blink_led_x_times(tick_blink_led_x_times, 350, 0, MILLIS);

void tick_blink_led_x_times()
{

  if (blink_led_x_times_counter % 2)
  {
    if (blink_led_x_times_toSendHsv)
    {
      set_single_RGB_color(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], 255);
    }
    else
    {
      set_single_RGB_color(my_rgb_hsv_values[0], my_rgb_hsv_values[1], 255);
    }
  }
  else
  {
    if (blink_led_x_times_toSendHsv)
    {
      set_single_RGB_color(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], 0);
    }
    else
    {
      set_single_RGB_color(my_rgb_hsv_values[0], my_rgb_hsv_values[1], 0);
    }
  }

  blink_led_x_times_counter--;
  if (blink_led_x_times_counter <= 0)
  {

    set_single_RGB_color(my_rgb_hsv_values[0], 0, 0);

    ticker_blink_led_x_times.stop();
    disable_touch_for_x_ms(500);
    rgb_led_is_busy_flag = false;
#ifdef DEBUG_AMOR
    Serial.println(F("==ticker_blink_led_x_times.stop(); =="));
    Serial.println(blink_led_x_times_counter);
#endif
  }
}

void blink_led_x_times(int x, bool toSendHsv)
{

  blink_led_x_times_counter = 2 * x;
  blink_led_x_times_toSendHsv = toSendHsv;

  ticker_blink_led_x_times.start();
  rgb_led_is_busy_flag = true;
#ifdef DEBUG_AMOR
  Serial.println(F("==ticker_blink_led_x_times.start(); =="));
  Serial.println(blink_led_x_times_counter);
#endif
}

void update_my_rgb_hsv(uint8_t h, uint8_t s, uint8_t v)
{
  my_rgb_hsv_values[0] = h;
  my_rgb_hsv_values[1] = s;

  // updatetoConfigJSON("myrgbHSL", hslN2S(h, s, v));  //TODO: is it required? ANS NO!

#ifdef DEBUG_AMOR
  Serial.println(F("Updated HSV my hsv"));
  Serial.println(my_rgb_hsv_values[0]);
  Serial.println(my_rgb_hsv_values[1]);
#endif
}

void update_tosend_rgb_hsv(uint8_t h, uint8_t s, uint8_t v)
{
  tosend_rgb_hsv_values[0] = h;
  tosend_rgb_hsv_values[1] = s;

  //updatetoConfigJSON("toSendHSL", hslN2S(h, s, v)); // TODO: to comment or update shadow?
  update_shadow_tosend_rgb_hsv_flag = true;

  // updateDeviceShadow("\"toSendHSL\":\"" + hslN2S(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], tosend_rgb_hsv_values[2]) + "\"");

#ifdef DEBUG_AMOR
  Serial.println(F("Updated HSV to send hsv"));
  Serial.println(tosend_rgb_hsv_values[0]);
  Serial.println(tosend_rgb_hsv_values[1]);
#endif
}

void update_x_min_on_value(int x)
{
  if (x_min_on_value > 0 && x_min_on_value < 61)
  {
    x_min_on_value = x;
  }
  else
  {
    x_min_on_value = 5;
  }

  // Update in config json

  updateDeviceShadow("\"x_min_on_value\":\"" + (String)x_min_on_value + "\"");

  bool ok = true; //updatetoConfigJSON("x_min_on_value", String(x));

#ifdef DEBUG_AMOR
  if (ok)
  {
    Serial.println(F("update_x_min_on_value"));
    Serial.println(readFromConfigJSON("x_min_on_value"));
  }
  else
  {
    Serial.println(F("FAILED x_min_on_value"));
    Serial.println(readFromConfigJSON("x_min_on_value"));
  }
#endif

  restart_device();
}

void update_groupId(String gID)
{
#ifdef DEBUG_AMOR
  Serial.println(F("update_groupId"));
  Serial.println(gID);
#endif

  groupId = gID;
  // Update in config json

  updateDeviceShadow("\"groupId\":\"" + gID + "\"");
  bool ok = true; //updatetoConfigJSON("groupId", gID);
  if (ok)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("update_groupId"));
    Serial.println(readFromConfigJSON("groupId"));
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println(F("FAILED update_groupId"));
    Serial.println(readFromConfigJSON("groupId"));
#endif
  }

  delay(1000);
  // ESP.restart();
  restart_device();
}

void method_handler(methodCode mc, int args, bool plus1arg, uint8_t s, uint8_t v)
{
  // methodCode
  turn_off_disco_mode();

  if (!plus1arg)
  {
    RGBQueueTask task((methodCode)mc, args);
    rgb_led_task_queue.push(&task);
  }
  else
  {
    RGBQueueTask task((methodCode)mc, args, s, v);
    rgb_led_task_queue.push(&task);
  }
}

String hslN2S(uint8_t h, uint8_t s, uint8_t l)
{
  String str_hsl = "";

  if (h < 10)
  {
    str_hsl = str_hsl + "00" + h;
  }
  else if (h < 100)
  {
    str_hsl = str_hsl + "0" + h;
  }
  else
  {
    str_hsl = str_hsl + "" + h;
  }

  if (s < 10)
  {
    str_hsl = str_hsl + "00" + s;
  }
  else if (s < 100)
  {
    str_hsl = str_hsl + "0" + s;
  }
  else
  {
    str_hsl = str_hsl + "" + s;
  }

  if (l < 10)
  {
    str_hsl = str_hsl + "00" + l;
  }
  else if (l < 100)
  {
    str_hsl = str_hsl + "0" + l;
  }
  else
  {
    str_hsl = str_hsl + "" + l;
  }

  return str_hsl;
}

void hslS2N(String mystr, uint8_t isToSendv)
{
  // 256256256
  uint8_t h = mystr.substring(0, 3).toInt();
  uint8_t s = mystr.substring(3, 6).toInt();
  uint8_t l = mystr.substring(6).toInt();

#ifdef DEBUG_AMOR
  Serial.println(F("== toSendHSL =="));
  Serial.println(h);
  Serial.println(s);
  Serial.println(l);
  Serial.println();
#endif

  switch (isToSendv)
  {
  case 0:
    my_rgb_hsv_values[0] = h;
    my_rgb_hsv_values[1] = s;
    my_rgb_hsv_values[2] = l;
    break;

  case 1:
    tosend_rgb_hsv_values[0] = h;
    tosend_rgb_hsv_values[1] = s;
    tosend_rgb_hsv_values[2] = l;
    break;

  default:
    break;
  };
}

void aws_callback(char *topic, byte *payload, unsigned int length)
{

  String topicStr = topic;

#ifdef DEBUG_AMOR
  Serial.println(F("aws_callback"));
  Serial.println(topicStr);
  Serial.print(F("payload  length>"));
  Serial.println(length);
  printHeap();

#endif

  if (topicStr.startsWith("amorgroup"))
  {
    // To handle JSON payload msgs
    StaticJsonDocument<256> doc;
    // Serial to JSON
    deserializeJson(doc, payload);

    if (topicStr.endsWith("tunnel"))
    {
      if (doc.containsKey("myDId") && doc["myDId"] != deviceId)
      {
#ifdef DEBUG_AMOR
        Serial.println(F("Incomming light msg from"));
        serializeJson(doc["myDId"], Serial);
        serializeJson(doc["et"], Serial);
#endif
        // check how old is msg , is it need to switch on the timer , then for how namy minutes ?
        // if et-msg + stay-on-min*60 > current et , then on th light  for remining mins
        // (et-msg + stay-on-min*60 - current et)/60
        //  5 + 60*60 - 3595 / 60
        //  10 /60
        // 1

        // if device is already on from previous touch then what to do ?

        // else as usual touchEpoch

        unsigned long current_et = timeClient.getEpochTime();

        if ((unsigned long)doc["et"] + (x_min_on_value * 60) > current_et)
        {
          if ((unsigned long)doc["et"] == touchEpoch)
          {
#ifdef DEBUG_AMOR
            Serial.println(F(" Already on because of this touch"));
#endif
          }
          else
          {
            // new touch or old touch jiska timer over nahi hua
            long timee2 = ((unsigned long)doc["et"] - current_et);

            // if msg recieved i not older than 60 seconds
            if (timee2 > -60 && timee2 <= 0)
            {
              timee2 = 0;
            }

            uint8_t ontimee = (uint8_t)((timee2 + (x_min_on_value * 60)) / 60);

            method_handler(MUMYRGB, (uint8_t)doc["c"][0], true, (uint8_t)doc["c"][1], 0);
            if (ticker_turn_on_RGB_led_for_x_mins.state() == RUNNING)
            {
#ifdef DEBUG_AMOR
              Serial.println(F("Already on , fade in out then on"));
#endif
              method_handler(MFADEINOUT, 2, true, 0, 0);
            }

            touchEpoch = (unsigned long)doc["et"];

#ifdef DEBUG_AMOR
            Serial.println(F("Turing on led for x mins "));
            Serial.println(touchEpoch);
            Serial.println(ontimee);

#endif
            method_handler(MXMINSON, ontimee, false, 0, 0);
          }
        }
        else
        {
#ifdef DEBUG_AMOR
          Serial.println(F(" THIS IS OLD TOUCH !!!"));
#endif
        }
      }
      else
      {
#ifdef DEBUG_AMOR
        Serial.println(F("Incomming light msg from self or it is null"));
        serializeJson(doc["myDId"], Serial);
#endif
      }
    }
  }
  else if (topicStr.startsWith("$aws/things/"))
  {
    if (topicStr.endsWith("rpc"))
    {
      rpc_method_handler(payload, length);
    }
    //$aws/things/amorAAA_123ABC/shadows
    else if (topicStr.startsWith("$aws/things/" + deviceId + "/shadow"))
    {
//topic is related to shadows
// shadow handler
#ifdef DEBUG_AMOR
      Serial.println(F("Shadow payload"));
#endif

      if (topicStr.endsWith("get/accepted"))
      {
        get_accepted_shadow_handler(payload, length);
      }
      else if (topicStr.endsWith("update/delta"))
      {
        update_delta_shadow_handler(payload, length);
      }
      //     else if (topicStr.endsWith("get/rejected"))
      //     {
      // #ifdef DEBUG_AMOR
      //       Serial.println(F("get/rejected"));
      // #endif
      //     }
      //     else if (topicStr.endsWith("update/rejected"))
      //     {
      // #ifdef DEBUG_AMOR
      //       Serial.println(F("update/rejected"));
      // #endif
      //     }
      else
      {
#ifdef DEBUG_AMOR
        Serial.println(F("Shadow payload"));
        Serial.println(topicStr);

        for (int i = 0; i < length; i++)
        {
          Serial.print((char)payload[i]);
        }

#endif
      }

#ifdef DEBUG_AMOR
      Serial.println(F("Shadow payload END"));
#endif
    }
  }
  else if (topicStr.endsWith(""))
  {
    // delete if not required in code clean up
  }
  else
  {
    // delete if not required in code clean up
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("Some topic"));
    Serial.println(topicStr);
#endif
  }
}

void get_accepted_shadow_handler(byte *payload, unsigned int length)
{
#ifdef DEBUG_AMOR
  Serial.println(F("get_accepted_shadow_handler method"));
  printHeap();
#endif

  StaticJsonDocument<512> doc;
  // Serial to JSON
  StaticJsonDocument<256> filter;

  // filter["state"]["reported"]["toSendHSL"] = true;
  // filter["state"]["reported"]["groupId"] = true;
  // filter["state"]["reported"]["x_min_on_value"] = true;

  filter["state"]["reported"] = true;

  deserializeJson(doc, payload, DeserializationOption::Filter(filter));

  if (doc["state"]["reported"].containsKey("toSendHSL"))
  {
    hslS2N(doc["state"]["reported"]["toSendHSL"], 1);
  }

  if (doc["state"]["reported"].containsKey("x_min_on_value"))
  {
    x_min_on_value = (int)doc["state"]["reported"]["x_min_on_value"];
  }

  if (doc["state"]["reported"].containsKey("groupId"))
  {
    groupId = doc["state"]["reported"]["groupId"].as<String>();
    aws_group_topic_str = "amorgroup/" + groupId + "/";
  }

#ifdef DEBUG_AMOR
  Serial.println(F("get_accepted_shadow_handler method"));
  printHeap();
  serializeJson(doc, Serial);
#endif

  // now subscribe group topics and publish aws data
  subscribeDeviceTopic_group();
  publish_boot_data();
}

void update_delta_shadow_handler(byte *payload, unsigned int length)
{
#ifdef DEBUG_AMOR
  Serial.println(F("update_delta_shadow_handler method"));
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
#endif
}

// method flags /enum
bool download_file_to_fs_flag = false;

void rpc_method_handler(byte *payload, unsigned int length)
{
  // To handle JSON payload msgs
  // 512 because it might have long urls
  StaticJsonDocument<512> doc;
  // Serial to JSON
  deserializeJson(doc, payload);

#ifdef DEBUG_AMOR
  Serial.println(F("making rpc calls method"));
  serializeJson(doc["method"], Serial);
#endif

  if (doc["method"] == "method_handler")
  {
    method_handler((methodCode)doc["mc"], (int)doc["args"], (bool)doc["plus1arg"], (uint8_t)doc["s"], (uint8_t)doc["v"]);
  }
  else if (doc["method"] == "turn_off_rgb")
  {
    turn_off_rgb();
  }
  else if (doc["method"] == "turn_off_disco_mode")
  {
    turn_off_disco_mode();
  }
  else if (doc["method"] == "turn_on_disco_mode_for_x_mins")
  {
    if (doc.containsKey("x"))
    {
      turn_on_disco_mode_for_x_mins((int)doc["x"]);
    }
    else
    {
      turn_on_disco_mode_for_x_mins(x_min_on_value);
    }
  }
  else if (doc["method"] == "send_responseToAWS")
  {
    send_responseToAWS(doc["responseMsg"]);
  }
  else if (doc["method"] == "send_touch_toGroup")
  {
    send_touch_toGroup();
  }
  else if (doc["method"] == "forget_saved_wifi_creds")
  {
    forget_saved_wifi_creds();
  }
  else if (doc["method"] == "update_x_min_on_value")
  {
    update_x_min_on_value((int)doc["x"]);
  }
  else if (doc["method"] == "update_groupId")
  {
    update_groupId(doc["gID"]);
  }
  else if (doc["method"] == "readFromConfigJSON")
  {
    send_responseToAWS(readFromConfigJSON(doc["key"]));
  }
  else if (doc["method"] == "readFrom_given_ConfigJSON")
  {
    String key = doc["key"];
    String filename = doc["filename"];
    send_responseToAWS(readFrom_given_ConfigJSON(key, filename));
  }
  else if (doc["method"] == "updatetoConfigJSON")
  {
    String key = doc["key"];
    String value = doc["value"];
    bool ok = updatetoConfigJSON(key, value);
#ifdef DEBUG_AMOR
    Serial.println(F("updatetoConfigJSON ok >"));
    Serial.println(ok);
#endif

    // send_responseToAWS(String(ok));
  }
  else if (doc["method"] == "updateto_givenfile_ConfigJSON")
  {
    String key = doc["key"];
    String value = doc["value"];
    String filename = doc["filename"];
    send_responseToAWS(updateto_givenfile_ConfigJSON(key, value, filename, (bool)doc["flag"]) ? key : "fail");
  }
  else if (doc["method"] == "removeFromConfigJSON")
  {
    bool ok = removeFromConfigJSON(doc["key"]);
    send_responseToAWS(String(ok));
  }
  else if (doc["method"] == "get_ESP_core")
  {
    String s = doc["key"];
    s.concat(" " + get_ESP_core(s));
    send_responseToAWS(s);
  }
  else if (doc["method"] == "restart_device")
  {
    restart_device();
  }
  else if (doc["method"] == "firmware_update_from_config")
  {
    firmware_update_from_config();
  }
  else if (doc["method"] == "firmware_update_from_fs")
  {
    firmware_update_from_fs(doc["ota_filename"]);
  }
  else if (doc["method"] == "download_file_to_fs")
  {
    // download_file_to_fs();
    download_file_to_fs_flag = true;
  }
  else if (doc["method"] == "delete_file_of_fs")
  {
    delete_file_of_fs(doc["filename"]);
  }
  else if (doc["method"] == "list_fs_files_sizes")
  {
    send_responseToAWS(list_fs_files_sizes());
  }
  else if (doc["method"] == "send_given_msg_to_given_topic")
  {
    send_given_msg_to_given_topic(doc["topic"], doc["msg"]);
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println(F("making rpc calls method  NOT FOUND"));
#endif
  }
}

// void subscribeDeviceTopics()
// {

// #ifdef DEBUG_AMOR
//   Serial.println(F("subscribeDeviceTopics before"));
//   Serial.println(aws_topic_str);
//   Serial.println(aws_group_topic_str);
// #endif

//   clientPubSub.subscribe((aws_topic_str + "+").c_str());
//   clientPubSub.subscribe((aws_group_topic_str + "+").c_str());

//   //Sending ip wo aws servers for debugging

//   send_responseToAWS(deviceId + " = " + readFromConfigJSON("localIP"));

// #ifdef DEBUG_AMOR
//   Serial.println(F("subscribeDeviceTopics DONEE ..."));
//   Serial.println(ESP.getFreeHeap());
// #endif
// }

void send_given_msg_to_given_topic(String topic, String msg)
{
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println(F("send_given_msg_to_given_topic()"));
  Serial.println(topic);
  Serial.println(msg);
#endif

  bool ok = clientPubSub.publish(topic.c_str(), msg.c_str());

#ifdef DEBUG_AMOR
  if (ok)
  {
    Serial.println(F("send_given_msg_to_given_topic sent OK "));
  }
  else
  {
    Serial.println(F("send_given_msg_to_given_topic sent failed!"));
  }
  printHeap();
#endif
}

void send_responseToAWS(String responseMsg)
{
//when calls via rpc are made
// this sends back response by publishing msg back to
//$aws/things/esp8266_C7ED21/response
#ifdef DEBUG_AMOR
  Serial.println(F("send_reposeToAWS"));
  printHeap();
#endif
  String et = String(timeClient.getEpochTime());
  String msg = "{\"respMsg\":\"" + responseMsg +
               "\",\"et\":\"" + et +
               "\",\"deviceId\":\"" + deviceId +
               "\"}";

#ifdef DEBUG_AMOR
  Serial.println(msg);
#endif

  bool ok = clientPubSub.publish((aws_topic_str + "response").c_str(), msg.c_str());

  if (ok)
  {
    method_handler(MBLEDX, 1, true, 1, 0);
  }

#ifdef DEBUG_AMOR
  Serial.println(ok ? "responseMsg sent OK " : "responseMsg sent failed!");
#endif
}

void send_touch_toGroup()
{
#ifdef DEBUG_AMOR
  Serial.println(F("send_touch_toGroup"));
  printHeap();
#endif
  //[255,255,255]
  String color = "[" + String(tosend_rgb_hsv_values[0]) + "," + tosend_rgb_hsv_values[1] + "]";

  String et = String(timeClient.getEpochTime());

  String msg = "{\"et\":\"" + et + "\",\"c\":" + color + ",\"myDId\":\"" + deviceId + "\"}";

#ifdef DEBUG_AMOR
  Serial.println(color);
  Serial.println(msg);
  printHeap();
#endif

  // bool ok = clientPubSub.publish(aws_group_topic_str.c_str(), msg.c_str());
  bool ok = clientPubSub.publish((aws_group_topic_str + "tunnel").c_str(), msg.c_str());

  if (ok)
  {
    // method_handler(MBLEDX, 3, true, 1, 0);
    method_handler(MFADEINOUT, 1, true, 1, 0);
  }

#ifdef DEBUG_AMOR
  Serial.println(ok ? "touch events updated!" : "touch events update failed!");
  printHeap();
#endif
}

// ---- CERTIFICATES READ for  AWS IOT SETUP START ----

void publish_boot_data()
{

#ifdef DEBUG_AMOR
  Serial.println(F("publis_boot_data()"));
  printHeap();
#endif

  /*publish kaise he karega jab shadow data wahan hai he nhi*/

  //   if (readFromConfigJSON("firstboot") == "true")
  //   {
  //     updateDeviceShadow("\"deviceId\": \"" + deviceId + "\",\"mac\":\"" + WiFi.macAddress() + "\",\"toSendHSL\":\"" + hslN2S(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], tosend_rgb_hsv_values[2]) + "\",\"groupId\":\"" + groupId + "\",\"localIP\":\"" + WiFi.localIP().toString() + "\",\"x_min_on_value\":\"" + x_min_on_value + "\"");
  //     updatetoConfigJSON("firstboot", "false");

  // #ifdef DEBUG_AMOR
  //     Serial.println(F(" updatetoConfigJSON('firstboot', 'false')"));
  //     printHeap();
  // #endif
  //   }

  unsigned long et = timeClient.getEpochTime();

  struct tm *ptm = gmtime((time_t *)&et);

  String msg = "{\"deviceId\":\"" + deviceId + "\",\"reconAwsCount\":\"" + recon_aws_count + "\",\"groupId\":\"" + groupId + "\",\"et\":\"" + et + "\",\"FW_ver\":\"" + FirmwareVer + "\",\"time\":\"" + timeClient.getFormattedTime() + "\",\"date\":\"" + (ptm->tm_mday) + "-" + (ptm->tm_mon + 1) + "-" + (ptm->tm_year + 1900) + "\",\"localIP\":\"" + WiFi.localIP().toString() + "\",\"resetInfo\":\"" + ESP.getResetInfo() + "\"}";

#ifdef DEBUG_AMOR
  Serial.println(msg);
#endif

  bool ok = clientPubSub.publish((aws_topic_str + "bootData").c_str(), msg.c_str());

  if (!ok)
  {
    delay(500);
    ok = clientPubSub.publish((aws_topic_str + "bootData").c_str(), msg.c_str());
  }

  if (ok)
  {
    method_handler(MBLEDX, 1, true, 1, 0);
  }

#ifdef DEBUG_AMOR
  Serial.println(ok ? "bootData sent OK " : "bootData sent failed!");
#endif
}

void getDeviceShadow()
{
#ifdef DEBUG_AMOR
  Serial.println(F("getDeviceShadow()"));
  printHeap();
#endif
  // if first boot thenfirst create shadow than update
  if (readFromConfigJSON("firstboot") == "true")
  {
    updateDeviceShadow("\"deviceId\": \"" + deviceId + "\",\"mac\":\"" + WiFi.macAddress() + "\",\"toSendHSL\":\"" + hslN2S(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], tosend_rgb_hsv_values[2]) + "\",\"groupId\":\"" + groupId + "\",\"localIP\":\"" + WiFi.localIP().toString() + "\",\"x_min_on_value\":\"" + x_min_on_value + "\"");
    updatetoConfigJSON("firstboot", "false");

#ifdef DEBUG_AMOR
    Serial.println(F(" updatetoConfigJSON('firstboot', 'false')"));
    printHeap();
#endif
  }

  // TODO: shoud we limit it on the basis of reconAwsCount?  or not
  if (recon_aws_count < 7)
  {
    clientPubSub.publish(("$aws/things/" + deviceId + "/shadow/name/configShadow/get").c_str(), "{}");
  }
#ifdef DEBUG_AMOR
  else
  {
    Serial.println(F("not clientPubSub.publish , recon_aws_count = "));
    Serial.println(recon_aws_count);
    printHeap();
  }
#endif
}

// void gId_or_ON_time_updateDeviceShadow()
// {
//   String shadowMsg = "{\"state\": {\"reported\": {\"groupId\":\"" + groupId + "\",\"x_min_on_value\":\"" + x_min_on_value + "\"}}}";
//   bool ok = clientPubSub.publish(("$aws/things/" + deviceId + "/shadow/name/configShadow/update").c_str(), shadowMsg.c_str());

// }

// void toSendHSL_updateDeviceShadow()
// {
//   String shadowMsg = "{\"state\": {\"reported\": {\"toSendHSL\":\"" + hslN2S(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], tosend_rgb_hsv_values[2]) + "\"}}}";
//   bool ok = clientPubSub.publish(("$aws/things/" + deviceId + "/shadow/name/configShadow/update").c_str(), shadowMsg.c_str());
// }

// void updateDeviceShadow()
// {
//   String shadowMsg = "{\"state\": {\"reported\": {\"deviceId\": \"" + deviceId + "\",\"mac\":\"" + WiFi.macAddress() + "\",\"toSendHSL\":\"" + hslN2S(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], tosend_rgb_hsv_values[2]) + "\",\"groupId\":\"" + groupId + "\",\"localIP\":\"" + WiFi.localIP().toString() + "\",\"x_min_on_value\":\"" + x_min_on_value + "\"}}}";
//   bool ok = clientPubSub.publish(("$aws/things/" + deviceId + "/shadow/name/configShadow/update").c_str(), shadowMsg.c_str());
// }

void update_shadow_tosend_rgb_hsv()
{
  if (millis() - update_shadow_tosend_rgb_hsv_last_millis > update_shadow_tosend_rgb_hsv_interval)
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("update_shadow_tosend_rgb_hsv()"));
    Serial.println(update_shadow_tosend_rgb_hsv_last_millis);
#endif

    update_shadow_tosend_rgb_hsv_flag = false;
    updateDeviceShadow("\"toSendHSL\":\"" + hslN2S(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], tosend_rgb_hsv_values[2]) + "\"");
    // String et = String(timeClient.getEpochTime());
    // String msg = "e" + et + "__" + hslN2S(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], tosend_rgb_hsv_values[2]);
    // clientPubSub.publish("helloramit", msg.c_str());
    update_shadow_tosend_rgb_hsv_last_millis = millis();
  }
}

void updateDeviceShadow(String shadowMsg)
{
  //\"deviceId\": \"" + deviceId + "\",\"mac\":\"" + WiFi.macAddress() + "\",\"toSendHSL\":\"" + hslN2S(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], tosend_rgb_hsv_values[2]) + "\",\"groupId\":\"" + groupId + "\",\"localIP\":\"" + WiFi.localIP().toString() + "\",\"x_min_on_value\":\"" + x_min_on_value + "\"
  // String shadowMsg = "{\"state\": {\"reported\": {" + shadowMsg + "}}}";
  bool ok = clientPubSub.publish(("$aws/things/" + deviceId + "/shadow/name/configShadow/update").c_str(), ("{\"state\": {\"reported\": {" + shadowMsg + "}}}").c_str());
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println(shadowMsg);
  ok ? Serial.println(F("ok 1 updateDeviceShadow()")) : Serial.println(F("not ok 0 updateDeviceShadow()"));
#endif
}

void subscribeDeviceShadow()
{
  clientPubSub.subscribe(("$aws/things/" + deviceId + "/shadow/name/configShadow/update/rejected").c_str());
  clientPubSub.subscribe(("$aws/things/" + deviceId + "/shadow/name/configShadow/update/delta").c_str());

  clientPubSub.subscribe(("$aws/things/" + deviceId + "/shadow/name/configShadow/get/+").c_str());
}

void subscribeDeviceTopic_group()
{
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println(F("subscribeDeviceTopic_group()"));
  Serial.println("amorgroup/" + groupId + "/+");
#endif

  clientPubSub.subscribe(("amorgroup/" + groupId + "/+").c_str());

#ifdef DEBUG_AMOR
  Serial.println(F("subscribeDeviceTopic_group() DONEE ..."));
  printHeap();
#endif
}

void subscribeDeviceTopics()
{
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println(F("subscribeDeviceTopics()"));
  Serial.println(aws_topic_str);
  Serial.println(aws_group_topic_str);
#endif

  // TODO: check + or # wild card ? ANS: +
  clientPubSub.subscribe((aws_topic_str + "+").c_str());
  // clientPubSub.subscribe((aws_group_topic_str + "+").c_str());

  // send_responseToAWS(deviceId + " = " + readFromConfigJSON("localIP"));

#ifdef DEBUG_AMOR
  Serial.println(F("^ old subscribeDeviceTopics() DONEE ..."));
  printHeap();
#endif
}

void readAwsCerts()
{
#ifdef DEBUG_AMOR
  Serial.print(F("Heap: "));
  Serial.println(ESP.getFreeHeap());
  printHeap();
#endif

  leds[NUM_LEDS - 1] = CRGB::Violet;
  leds[1] = CRGB::Violet;
  FastLED.show();

  // TODO:verify that weather closing the opened file later on makes any difference
  //   if (!fileSystem->begin())
  //   {
  // #ifdef DEBUG_AMOR
  //     printHeap();
  //     Serial.println(F("Failed to mount file system"));
  // #endif
  //     return;
  //   }

  // Load certificate file
  File cert = fileSystem->open(readFromConfigJSON("cert_cert"), "r"); //replace cert.crt with your uploaded file name
  if (!cert)
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("Failed to open cert file"));
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("Success to open cert file"));
#endif
  }

  delay(1000);

  if (espClient.loadCertificate(cert))
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("cert loaded"));
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("cert not loaded"));
#endif
  }

  cert.close();

  // Load private key file
  File private_key = fileSystem->open(readFromConfigJSON("cert_private"), "r"); //replace private with your uploaded file name
  if (!private_key)
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("Failed to open private cert file"));
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("Success to open private cert file"));
#endif
  }

  delay(1000);

  if (espClient.loadPrivateKey(private_key))
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("private key loaded"));
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("private key not loaded"));
#endif
  }

  private_key.close();

  // Load CA file
  File ca = fileSystem->open(readFromConfigJSON("cert_ca"), "r"); //replace ca with your uploaded file name
  if (!ca)
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("Failed to open ca "));
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("Success to open ca"));
#endif
  }

  delay(1000);

  if (espClient.loadCACert(ca))
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("ca loaded"));
#endif
  }

  else
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("ca failed"));
#endif
  }

  ca.close();

#ifdef DEBUG_AMOR
  printHeap();
#endif

  // fileSystem->end();

#ifdef DEBUG_AMOR
  printHeap();
#endif
}

bool updateto_givenfile_ConfigJSON(String &key, String &value, String &filename, bool isToDelete)
{

  File configFile;

  configFile = fileSystem->open(filename, "r");
  if (!configFile)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Failed to open config file"));
#endif
    return false;
  }

  size_t size = configFile.size();
#ifdef DEBUG_AMOR
  Serial.print(F("Config file size read only="));
  Serial.println(size);
  printHeap();
#endif

  // totalSize = totalSize + size;

  // Serial.print(F("TOTAL file size FINAL !!! ="));
  // Serial.println(totalSize);

  if (size > 1024 * 3)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Config file size is too large"));
#endif
    return false;
  }

#ifdef DEBUG_AMOR
  Serial.println(F("Allocate a buffer START"));
  printHeap();
#endif
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

#ifdef DEBUG_AMOR
  Serial.println(F("Allocate a buffer END"));
  printHeap();
#endif

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonDocument<1024> doc;

#ifdef DEBUG_AMOR
  Serial.println("doc.memoryUsage()");
  Serial.println(doc.memoryUsage());
  printHeap();
#endif

  DeserializationError error = deserializeJson(doc, buf.get());

#ifdef DEBUG_AMOR
  Serial.println("doc.memoryUsage()");
  Serial.println(doc.memoryUsage());
  printHeap();
#endif

#ifdef DEBUG_AMOR //
  if (error)
  {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
  }
  serializeJsonPretty(doc, Serial); // TODO : delete it only to getconfig file data
#endif
  configFile.close();

  if (isToDelete)
  {
    if (doc.containsKey(key.c_str()))
    {
      doc.remove(key.c_str());
    }
    isToDeleteupdatetoConfigJSONflag = false;
  }
  else
  {
    doc[key.c_str()] = value;
  }

#ifdef DEBUG_AMOR //
  Serial.print(F("deserializeJson() After update "));
  serializeJsonPretty(doc, Serial); // TODO : delete it only to getconfig file data
#endif

  // configFile is closed after reading data into doc , now updating the data;

  File configFile_w = fileSystem->open(filename, "w"); // TODO: w or w+ ?

  if (!configFile_w)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Failed to open config file w"));
#endif
    configFile_w = fileSystem->open(filename, "w+");
    if (!configFile_w)
    {
#ifdef DEBUG_AMOR
      Serial.println(F("Failed to open config file in w+"));
#endif

      return false;
    }
  }

  size = configFile_w.size();

#ifdef DEBUG_AMOR
  Serial.print(F("Config file size write ="));
  Serial.println(size);
#endif

  if (size > 1024 * 3)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Config file size is too large"));
#endif
    return false;
  }

// TODO: check wheather it writes to to serial usb port or serially to flash chip
#ifdef DEBUG_AMOR
  Serial.println(F("serializeJson(doc, configFile);"));
  printHeap();
#endif

  serializeJson(doc, configFile_w);

  configFile_w.close();

  return true;
}

bool updatetoConfigJSON(String key, String value)
{

#ifdef DEBUG_AMOR
  printHeap();
  Serial.println("START updatetoConfigJSON " + key + ":" + value);
#endif
  // 10 config files ranges from 0 - 9
  char *fileName = "/config0.json";

  // (char)48 == '0' (char)57 == '9'

  String tempStr(fileName);

  for (uint8_t i = 48; i < 58; i++)
  {
    fileName[7] = (char)i;
    tempStr = fileName;
    tempStr = readFrom_given_ConfigJSON(key, tempStr);

    if (!tempStr.startsWith("ERR-"))
    {
      tempStr = fileName;
#ifdef DEBUG_AMOR
      printHeap();
      Serial.println("END updatetoConfigJSON " + key + ":" + value + tempStr);
#endif
      return updateto_givenfile_ConfigJSON(key, value, tempStr, isToDeleteupdatetoConfigJSONflag);
      //break;
    }
  }

  tempStr = fileName; // last value will be "/config9.json";
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println("END updatetoConfigJSON" + key + ":" + value + tempStr);
#endif
  return updateto_givenfile_ConfigJSON(key, value, tempStr, isToDeleteupdatetoConfigJSONflag);

  // in future will write to any one of 7,8,9 config files only , on basis of size;

  // key not found in any of the file
  // "ERR-KEY";
}

//deletes first occurance of key in first file
bool removeFromConfigJSON(String key)
{
  isToDeleteupdatetoConfigJSONflag = true;
  updatetoConfigJSON(key, "");
  isToDeleteupdatetoConfigJSONflag = false;
}

String readFrom_given_ConfigJSON(String &key, String &filename)
{
#ifdef DEBUG_AMOR
  printHeap();
  // Serial.println("readFrom_given_ConfigJSON" + key + " from " + filename);
#endif
  // TODO: check is little fs is begun before this call ?

  File configFile = fileSystem->open(filename, "r");
  if (!configFile)
  {
#ifdef DEBUG_AMOR
    // Serial.println(F("Failed to open config file"));
#endif
    // return (String) false;
    return "ERR-FO";
  }

  size_t size = configFile.size();

#ifdef DEBUG_AMOR
  // printHeap();
  // Serial.print(F("Config file size="));
  // Serial.println(size);
#endif

  // totalSize = totalSize + size;

  // Serial.print(F("TOTAL file size FINAL !!! ="));
  // Serial.println(totalSize);

  if (size > 1024 * 3)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Config file size is too large"));
    Serial.print(F("Config file size="));
    Serial.println(size);
#endif
    // return (String) false;
    configFile.close();
    return "ERR-FSL";
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  // printHeap();

  StaticJsonDocument<512> doc;

  StaticJsonDocument<256> filter;

  // printHeap();
  filter[key] = true;
  auto error = deserializeJson(doc, buf.get(), DeserializationOption::Filter(filter));

  // printHeap();

  // auto error = deserializeJson(doc, buf.get());

// printing json data to Serial Port in pretty format
#ifdef DEBUG_AMOR
  // Serial.println(F(""));
  // serializeJsonPretty(doc, Serial);
  // Serial.println(F(""));
#endif

  if (error)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Failed to parse config file"));
#endif
    //return false;
    // return (String)value;
    configFile.close();
    return "ERR-FPF";
  }
  if (doc.containsKey(key))
  {
    const char *value = doc[key];
#ifdef DEBUG_AMOR
    Serial.print(F("Loaded "));
    Serial.print(key);
    Serial.print(F(":"));
    Serial.println(value);
    // #endif
    // return true;
    printHeap();
#endif
    configFile.close();
    return value;
  }
  else
  {
    // is file mai key he nhi hai
    configFile.close();
    return "ERR-KEY";
  }

  // String value = doc[key]; // This didn't worked out

  // Real world application would store these values in some variables for
  // later use.
}

String readFromConfigJSON(String key)
{
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println("START readFromConfigJSON " + key);
#endif
  // 10 config files ranges from 0 - 9
  char *fileName = "/config0.json";

  // (char)48 == '0' (char)57 == '9'

  for (size_t i = 48; i < 58; i++)
  {
    fileName[7] = (char)i;
    String tempStr(fileName);
    tempStr = readFrom_given_ConfigJSON(key, tempStr);

    if (!tempStr.startsWith("ERR-"))
    {
#ifdef DEBUG_AMOR
      printHeap();
      Serial.println("END  readFromConfigJSON key=" + key + " val= " + tempStr);
#endif
      return tempStr;
    }
  }
  // key nt found in any of the file
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println("END  readFromConfigJSON" + key + " ERR-KEY");
#endif

  return "ERR-KEY";
}

void ws_rpc_method_handler(uint8_t num, byte *payload, unsigned int length)
{
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println("ws_rpc_method_handler");
#endif

  // To handle JSON payload msgs
  StaticJsonDocument<256> doc;
  // Serial to JSON
  payload++; //because first char is 'w';
  deserializeJson(doc, payload);

  String wsReturnStr;

#ifdef DEBUG_AMOR
  Serial.println(F("making ws rpc calls method"));
  serializeJsonPretty(doc, Serial);
  serializeJson(doc["method"], Serial);
#endif

  String s = doc["key"];

  if (doc["method"] == "readFromConfigJSON")
  {
    s.concat(" " + readFromConfigJSON(s));
    wsReturnStr = s;
  }
  else if (doc["method"] == "ws_update_tosend_color")
  {
    method_handler(MUTOSENDRGB, (uint8_t)doc["h"], true, (uint8_t)doc["s"], 0);
    method_handler(MBLEDX, 3, true, 1, 0); // blinking removed
    // fading added
    s = "c ok";
    wsReturnStr = s;
  }
  else if (doc["method"] == "get_ESP_core")
  {
    s.concat(" " + get_ESP_core(s));
    wsReturnStr = s;
  }
  else if (doc["method"] == "list_fs_files_sizes")
  {
    wsReturnStr = list_fs_files_sizes();
  }
  else if (doc["method"] == "update_x_min_on_value")
  {
    update_x_min_on_value((int)doc["x"]);
  }
  else if (doc["method"] == "update_groupId")
  {
    update_groupId(doc["gID"]);
  }
  else if (doc["method"] == "updatetoConfigJSON")
  {
    String key = doc["key"];
    String value = doc["value"];
    bool ok = updatetoConfigJSON(key, value);
#ifdef DEBUG_AMOR
    Serial.println(F("updatetoConfigJSON ok >"));
    Serial.println(ok);
#endif

    wsReturnStr = ok ? "ok updated" : "not updated";
  }
  else
  {
    wsReturnStr = "invalid method";
#ifdef DEBUG_AMOR
    Serial.println(F("making ws rpc calls method  NOT FOUND"));
#endif

    //TODO: is it required ? check heap while execution. , this is not working , find issue.
    // rpc_method_handler(payload, length); // disabled code
  }
  webSocket.sendTXT(num, wsReturnStr.c_str(), wsReturnStr.length()); //TODO: length or length+1
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
#ifdef DEBUG_AMOR
  Serial.println(F("webSocketEvent"));
  // Serial.println(num);
  // Serial.println(type);

  for (size_t i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }

  Serial.println();
  printHeap();
#endif

  if (type == WStype_TEXT)
  {
    if (payload[0] == 'w')
    {
      //ws is expecting some response
      ws_rpc_method_handler(num, payload, length);
    }
    else
    {
      rpc_method_handler(payload, length);
      webSocket.sendTXT(num, "ok", 3);
    }
  }
  else if (type == WStype_CONNECTED)
  {
    webSocket.sendTXT(num, "ok connected", 13);
    String wsData = lamp_info_string();

    // String wsData = "{\"deviceId\":\"" + deviceId +
    //                 "\",\"wsData\":\"true\",\"mac\":\"" + WiFi.macAddress() +
    //                 "\",\"toSendHSL\":\"" + hslN2S(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], tosend_rgb_hsv_values[2]) +
    //                 "\",\"hue\":\"" + tosend_rgb_hsv_values[0] +
    //                 "\",\"sat\":\"" + tosend_rgb_hsv_values[1] +
    //                 "\",\"groupId\":\"" + groupId +
    //                 "\",\"localIP\":\"" + WiFi.localIP().toString() +
    //                 "\",\"x_min_on_value\":\"" + x_min_on_value +
    //                 "\",\"reconAwsCount\":\"" + recon_aws_count +
    //                 "\",\"groupId\":\"" + groupId +
    //                 "\",\"FW_ver\":\"" + FirmwareVer +
    //                 "\",\"resetInfo\":\"" + ESP.getResetInfo() + "\"}";
    // ;

    webSocket.sendTXT(num, wsData.c_str(), wsData.length());
#ifdef DEBUG_AMOR
    Serial.println(F("ok ws connected num:-"));
    Serial.println(num);
#endif
  }
  // else if(type == WStype_DISCONNECTED){

  // }else if(type == WStype_ERROR){

  // }else if(type == WStype_PING){
  //   // webSocket.sendPing(num, "ok ping");
  //   // webSocket.broadcastPing("ok",3);

  // }else if(type == WStype_PONG){
  //   webSocket.sendTXT(num, "ok", 3);

  // }
}

String lamp_info_string()
{
  String wsData = "{\"deviceId\":\"" + deviceId +
                  "\",\"wsData\":\"true\",\"mac\":\"" + WiFi.macAddress() +
                  "\",\"toSendHSL\":\"" + hslN2S(tosend_rgb_hsv_values[0], tosend_rgb_hsv_values[1], tosend_rgb_hsv_values[2]) +
                  "\",\"hue\":\"" + tosend_rgb_hsv_values[0] +
                  "\",\"sat\":\"" + tosend_rgb_hsv_values[1] +
                  "\",\"groupId\":\"" + groupId +
                  "\",\"localIP\":\"" + WiFi.localIP().toString() +
                  "\",\"x_min_on_value\":\"" + x_min_on_value +
                  "\",\"reconAwsCount\":\"" + recon_aws_count +
                  "\",\"groupId\":\"" + groupId +
                  "\",\"FW_ver\":\"" + FirmwareVer +
                  "\",\"resetInfo\":\"" + ESP.getResetInfo() + "\"}";

  return wsData;
}

void replyOK()
{
  server.send(200, FPSTR("text/plain"), "ok!");
}

void replyOKWithMsg(String msg)
{
  server.send(200, FPSTR("text/plain"), msg);
}

void replyServerError(String msg)
{
#ifdef DEBUG_AMOR
  Serial.println(msg);
#endif

  server.send(500, FPSTR("text/plain"), msg + "\r\n");
}

/*
   Handle a file upload request
*/

void handleFileUpload()
{
#ifdef DEBUG_AMOR
  Serial.println(F("handleFileUpload() START"));
  printHeap();
#endif

  String path = server.uri();

#ifdef DEBUG_AMOR
  Serial.println(String("handleFileRead: ") + path);
#endif

  if (!fsOK)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("error in opening FS"));
#endif

    return replyServerError(FPSTR("FS_INIT_ERROR"));
  }

  if (server.uri() != "/fsupload")
  {
#ifdef DEBUG_AMOR
    Serial.println(F("path not ends with /fsupload"));
#endif
    return;
  }

  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START)
  {

    String filename = upload.filename;

    // Make sure paths always start with "/"
    if (!filename.startsWith("/"))
    {
      filename = "/" + filename;
    }

#ifdef DEBUG_AMOR
    Serial.println(String("handleFileUpload Name: ") + filename);
#endif

    uploadFile = fileSystem->open(filename, "w+");

    if (!uploadFile)
    {
#ifdef DEBUG_AMOR
      Serial.println(String(" CREATE FAILED , filename: ") + filename);
#endif
      return replyServerError(F("CREATE FAILED"));
    }

#ifdef DEBUG_AMOR
    Serial.println(String("Upload: START, filename: ") + filename);
#endif
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("upload.status == UPLOAD_FILE_WRITE"));
#endif
    if (uploadFile)
    {
      size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);

#ifdef DEBUG_AMOR
      Serial.print(F("bytesWritten>>"));
      Serial.println(bytesWritten);
#endif

      if (bytesWritten != upload.currentSize)
      {
#ifdef DEBUG_AMOR
        Serial.println(F("WRITE FAILED !!!"));
#endif
        return replyServerError(F("WRITE FAILED"));
      }
    }
#ifdef DEBUG_AMOR
    Serial.println(String("Upload: WRITE, Bytes: ") + upload.currentSize);
#endif
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (uploadFile)
    {
      uploadFile.close();
#ifdef DEBUG_AMOR
      Serial.println(F("uploadFile.close(); PROPERLY"));
#endif
    }
#ifdef DEBUG_AMOR
    Serial.println(String("Upload: END/COMPLETED, Size: ") + upload.totalSize);
#endif
  }
#ifdef DEBUG_AMOR
  Serial.println(F("handleFileUpload() END"));
#endif
}

/*
   Read the given file from the filesystem and stream it back to the client
*/
// void handleFileRead(String path)
void handleFileRead()
{
#ifdef DEBUG_AMOR
  Serial.println(F("handleFileRead() START"));
#endif

  String path = server.uri();

#ifdef DEBUG_AMOR
  Serial.println(path);
  Serial.println(String("handleFileRead: ") + path);
#endif

  if (!fsOK)
  {
    replyServerError(FPSTR("FS_INIT_ERROR"));
    return;
  }

  if (!server.hasArg("filename"))
  {
    replyServerError(FPSTR("argument not found"));
    return;
  }

  String filename = server.arg("filename");

  if (path.endsWith("/"))
  {
    path += "index.html";
  }

  String contentType;
  if (server.hasArg("download"))
  {
    contentType = F("application/octet-stream");
  }
  else
  {
    contentType = mime::getContentType(filename);
  }

  if (!fileSystem->exists(filename))
  {
    // File not found, try gzip version
    filename = filename + ".gz";
  }
  if (fileSystem->exists(filename))
  {
    File file = fileSystem->open(filename, "r");
    if (server.streamFile(file, contentType) != file.size())
    {
#ifdef DEBUG_AMOR
      Serial.println(F("Sent less data than expected!"));
#endif
    }
    file.close();
#ifdef DEBUG_AMOR
    Serial.println(F(" file.close();"));
#endif

    // return true;
    return;
  }
  // return false;
  replyOKWithMsg("Something is wrong");

#ifdef DEBUG_AMOR
  Serial.println(F("handleFileRead() END"));
#endif
}

void handleNotFound()
{
#ifdef DEBUG_AMOR
  Serial.println(F("handleNotFound()"));
#endif

  String uri = ESP8266WebServer::urlDecode(server.uri()); // required to read paths with blanks

  // if (handleFileRead(uri))
  // {
  //   return;
  // }

  String message = "File Not Found\n\n";
  message.concat("URI: ");
  message.concat(server.uri());
  message.concat("\nMethod: ");
  message.concat((server.method() == HTTP_GET) ? "GET" : "POST");
  message.concat("\nArguments: ");
  message.concat(server.args());
  message.concat("\n");
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message.concat(" ");
    message.concat(server.argName(i));
    message.concat(": ");
    message.concat(server.arg(i));
    message.concat("\n");

    // message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
#ifdef DEBUG_AMOR
  Serial.println(message);
#endif
  server.send(404, "text/plain", message);
}

// TODO: setup mdns
void websocket_server_mdns_setup()
{
#ifdef DEBUG_AMOR
  Serial.println(F("websocket_server_mdns_setup()"));
  printHeap();
#endif
  if (WiFi.status() == WL_CONNECTED)
  {
    // server_setup();
    server.on("/ping", []() {
      // server.send_P(200, "text/html", webpage);
      // server.send_P(200, "text/html", lamp_info_string().c_str());
      server.send_P(200, "application/json", lamp_info_string().c_str());
    });

    server.on("/fs", []() {
      server.send(200, "text/html", F("<form name='myUploadForm' id='myUploadForm' method='POST' action='/fsupload' enctype='multipart/form-data'><div><label for='filename'>Select file</label><input name='filename' id='filename' type='file'></div><div><label for='fsupload'>PerformUpload</label><input name='fsupload' id='fsupload' type='submit' value='Update'></div></form><br/><form name='myReadForm' id='myReadForm' method='POST' action='/fsread' enctype='multipart/form-data'><div><label for='filename'>Select file</label><input name='filename' id='filename' type='text'></div><div><label for='fsread'>PerformRead</label><input name='fsread' id='fsread' type='submit' value='Read'></div></form>"));
    });

    // server.on("/fsread", HTTP_POST, replyOK, handleFileRead);
    // server.on("/fsupload", HTTP_POST, replyOK, handleFileUpload);

    server.on("/fsread", HTTP_POST, handleFileRead, replyOK);
    // server.on("/fsupload", HTTP_POST, handleFileUpload);

    server.on(
        "/fsupload", HTTP_POST, []() { // If a POST request is sent to the /edit.html address,
          server.send(200, "text/plain", "ok!");
        },
        handleFileUpload);

    // server.serveStatic("/index", LittleFS, "/index.html");
    // server.serveStatic("/style", LittleFS, "/style.css");
    // server.serveStatic("/script", LittleFS, "/script.js");
    // server.serveStatic("/config", LittleFS, "/config.json");
    // server.serveStatic("/dev", LittleFS, "/dev.html");

    server.serveStatic("/", LittleFS, "/index.html");
    server.serveStatic("/index", LittleFS, readFromConfigJSON("s_index").c_str());
    server.serveStatic("/style", LittleFS, readFromConfigJSON("s_style").c_str());
    server.serveStatic("/script", LittleFS, readFromConfigJSON("s_script").c_str());
    server.serveStatic("/config", LittleFS, readFromConfigJSON("s_config").c_str());
    server.serveStatic("/dev", LittleFS, readFromConfigJSON("s_dev").c_str());

    //webpageOTA
    server.on("/update", HTTP_GET, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", webpageOTA);
    });

    //handles webpage based OTA bin
    server.on(
        "/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart(); }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {

#ifdef DEBUG_AMOR
        Serial.setDebugOutput(true);
#endif

        WiFiUDP::stopAll();

#ifdef DEBUG_AMOR
        Serial.printf("Update: %s\n", upload.filename.c_str());
#endif

        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) { //start with max available size
#ifdef DEBUG_AMOR
          Update.printError(Serial);
#endif
yield();
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
#ifdef DEBUG_AMOR
          Update.printError(Serial);
#endif
          yield();
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
#ifdef DEBUG_AMOR
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
#endif
        yield();
        } else {
#ifdef DEBUG_AMOR
          Update.printError(Serial);
#endif
        }
#ifdef DEBUG_AMOR
        Serial.setDebugOutput(false);
#endif
      }
      yield(); });

    server.onNotFound(handleNotFound);
    server.begin();

    webSocket.begin();

    webSocket.onEvent(webSocketEvent);

    if (!MDNS.begin("setupamor"))
    {
#ifdef DEBUG_AMOR // Start the mDNS responder for setupamor.local
      Serial.println("Error setting up MDNS responder!");
#endif
    }
    else
    {
#ifdef DEBUG_AMOR
      Serial.println("mDNS responder started");
#endif
    }
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println(F("websocket_server_mdns_setup  !WL_CONNECTED"));
    printHeap();
#endif
    restart_device();
  }
#ifdef DEBUG_AMOR
  Serial.println(F("websocket_server_mdns_setup() done"));
  printHeap();
#endif
}

// TODO: delete in code clean up
// // DigiCert High Assurance EV Root CA
// const char trustRoot[] PROGMEM = R"EOF(
// -----BEGIN CERTIFICATE-----
// MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBs
// MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
// d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j
// ZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDTMxMTExMDAwMDAwMFowbDEL
// MAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3
// LmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2Ug
// RVYgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm
// +9S75S0tMqbf5YE/yc0lSbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTW
// PNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtqumX8OkhPhPYlG++MXs2ziS4wblCJEM
// xChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0AFFoEt7oT61EKmEFB
// Ik5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx+mM0aBhakaHPQNAQTXKFx01p8VdteZOE3
// hzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsg
// EsxBu24LUTi4S8sCAwEAAaNjMGEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQF
// MAMBAf8wHQYDVR0OBBYEFLE+w2kD+L9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaA
// FLE+w2kD+L9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3DQEBBQUAA4IBAQAcGgaX3Nec
// nzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe/EW1ntlMMUu4kehDLI6z
// eM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jF
// hS9OMPagMRYjyOfiZRYzy78aG6A9+MpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2
// Yzi9RKR/5CYrCsSXaQ3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2/S6cCZdkGCe
// vEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep
// +OkuE6N36B9K
// -----END CERTIFICATE-----
// )EOF";

void delete_file_of_fs(String filename)
{
#ifdef DEBUG_AMOR
  Serial.print(F("delete_file_of_fs()"));
  Serial.print(filename);
  printHeap();
#endif
  // Make sure paths always start with "/"
  if (!filename.startsWith("/"))
  {
    filename = "/" + filename;
  }

  bool delFlag = false;
  Dir dir = fileSystem->openDir("/");
  while (dir.next())
  {
    if (dir.fileName().equals(filename))
    {
      delFlag = true;
      break;
    }
    // yield();
  }

  if (delFlag)
  {
    fileSystem->remove(filename);
    return;
#ifdef DEBUG_AMOR
    Serial.print(F("delete_file_of_fs() END ,filefound !!!"));
    Serial.print(filename);
    printHeap();
#endif
  }

#ifdef DEBUG_AMOR
  Serial.print(F("delete_file_of_fs() END ,file not found"));
  Serial.print(filename);
  printHeap();
#endif
}

void download_file_to_fs()
{
#ifdef DEBUG_AMOR
  Serial.print(F("download_file_to_fs()"));
  printHeap();
#endif

  espClient.stopAll();

#ifdef DEBUG_AMOR
  printHeap();
#endif

  webSocket.~WebSocketsServer();
  clientPubSub.~PubSubClient();
  //
  server.~ESP8266WebServerTemplate();
  webSocket.~WebSocketsServer();

#ifdef DEBUG_AMOR
  Serial.println(F("~PubSubClient"));
  printHeap();
#endif

  // BearSSL::CertStore certStore;

  // String FirmwareVer = {"1.8"};
  // String URL_fw_Version = "/ramitdour/otaTest/master/myBigFile.txt";
  // String URL_fw_Bin = "https://raw.githubusercontent.com/programmer131/otaFiles/master/firmware.bin";
  // const char *host = "raw.githubusercontent.com";
  // const int httpsPort = 443;

  // String FirmwareVer = {"1.8"};

  String host_ca = readFromConfigJSON("d2fs_host_ca");          //ota.der
  String host = readFromConfigJSON("d2fs_host_url");            // "raw.githubusercontent.com"
  int httpsPort = readFromConfigJSON("d2fs_host_port").toInt(); // 443
  String d2fs_url_file = readFromConfigJSON("d2fs_url_file");   // without host name
  bool d2fs_finished_flag = readFromConfigJSON("d2fs_finished_flag").toInt();
  String filename = readFromConfigJSON("d2fs_filename");

  if (!filename.startsWith("/"))
  {
    filename = "/" + filename;
  }

#ifdef DEBUG_AMOR
  Serial.print(F("host_ca >"));
  Serial.println(host_ca);
  Serial.print(F("host >"));
  Serial.println(host);
  Serial.print(F("httpsPort >"));
  Serial.println(httpsPort);
  Serial.print(F("d2fs_url_file >"));
  Serial.println(d2fs_url_file);
  Serial.print(F("d2fs_finished_flag >"));
  Serial.println(d2fs_finished_flag);
  Serial.print(F("filename >"));
  Serial.println(filename);
  printHeap();
#endif

  if (host_ca.length() < 2 ||
      host.length() < 2 ||
      httpsPort < 2 ||
      d2fs_url_file.length() < 2 ||
      filename.length() < 2 ||
      FirmwareVer.length() < 2)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Something is null"));
#endif
    return;
  }

  // Load private key file
  if (!host_ca.startsWith("/"))
  {
    host_ca = "/" + host_ca;
  }

#ifdef DEBUG_AMOR
  Serial.print(F("host_ca > = "));
  Serial.println(host_ca);

  printHeap();
#endif

  File ota_ca_file = fileSystem->open(host_ca, "r"); //replace private with your uploaded file name
  if (!ota_ca_file)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Failed to open host_ca cert file"));
    printHeap();
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Success to open host_ca cert file"));
    printHeap();
#endif
  }

  delay(1000);

  if (espClient.loadCACert(ota_ca_file))
  {
#ifdef DEBUG_AMOR
    Serial.println(F("host_ca cert loaded"));
    printHeap();
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println(F("host_ca cert not loaded"));
    printHeap();
#endif
  }

  ota_ca_file.close();

#ifdef DEBUG_AMOR
  Serial.println(F("ota_ca_file.close();"));
  printHeap();
#endif

  // X509List cert(trustRoot);

  // WiFiClientSecure espClient;

#ifdef DEBUG_AMOR
  printHeap();
#endif

  // espClient.setTrustAnchors(&cert);

  if (!espClient.connect(host, httpsPort))
  {

#ifdef DEBUG_AMOR
    Serial.println(F("Connection to host failed"));
    printHeap();
#endif

    return;
  }

#ifdef DEBUG_AMOR
  printHeap();
#endif

  espClient.print(String("GET ") + d2fs_url_file + " HTTP/1.1\r\n" +
                  "Host: " + host + "\r\n" +
                  "User-Agent: BuildFailureDetectorESP8266\r\n" +
                  "Connection: keep-alive\r\n\r\n");

#ifdef DEBUG_AMOR
  printHeap();
#endif
  int size = 0;

  //   while (espClient.connected())
  //   {
  // #ifdef DEBUG_AMOR
  //     printHeap();
  // #endif
  //     String line = espClient.readStringUntil('\n');

  //     if (line == "\r")
  //     {
  // #ifdef DEBUG_AMOR
  //       Serial.println(F("Headers received"));
  //       printHeap();
  // #endif
  //       break;
  //     }

  //     char buf[128];
  //     espClient.readBytes(buf, 120);

  //     String x = buf;
  //     Serial.println(x);

  //     // Serial.println(F("+++++++Started++++++++"));
  //     // Serial.print(String(buf));
  //     // Serial.println(F("+++++++END++++++++"));
  //   }

  //   espClient.status();

  //   // String payload = espClient.readStringUntil('\n');

  int contentLength = -1;
  int httpCode;
  espClient.setInsecure(); // TODO: what this does ?

  while (espClient.connected())
  {
    String header = espClient.readStringUntil('\n');
#ifdef DEBUG_AMOR
    Serial.print(F("h..."));
    Serial.println(header);
#endif
    if (header.startsWith(F("HTTP/1.")))
    {
      httpCode = header.substring(9, 12).toInt();
#ifdef DEBUG_AMOR
      Serial.print(F("httpCode ..>>"));
      Serial.println(httpCode);
#endif

      if (httpCode != 200)
      {
#ifdef DEBUG_AMOR
        Serial.println(String(F("HTTP GET code=")) + String(httpCode));
#endif
        espClient.stop();
        // return -1;
        return;
      }
    }
    if (header.startsWith(F("Content-Length: ")))
    {
      contentLength = header.substring(15).toInt();
#ifdef DEBUG_AMOR
      Serial.print(F("contentLength ..>>"));
      Serial.println(contentLength);
#endif
    }
    if (header == F("\r"))
    {
      break;
    }
    // yield();
  }

  if (!(contentLength > 0))
  {
#ifdef DEBUG_AMOR
    Serial.println(F("HTTP content length=0"));
#endif
    espClient.stop();
    // return -1;
    return;
  }

  // Open file for write
  updatetoConfigJSON("d2fs_finished_flag", "0");

  fs::File f = fileSystem->open(filename, "w+");
  if (!f)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("file open failed"));
#endif
    espClient.stop();
    // return -1;
    return;
  }

  // Download file
  int remaining = contentLength;
  int received;
  uint8_t buff[256] = {0};

#ifdef DEBUG_AMOR
  Serial.println(F("file opened now  ,writing data"));
  Serial.println(filename);
  Serial.println(remaining);
#endif

  // read all data from server
  while (remaining > 0 && espClient.status() == ESTABLISHED)
  {
    // read up to buffer size
    received = espClient.readBytes(buff, ((remaining > sizeof(buff)) ? sizeof(buff) : remaining));

    // write it to file
    f.write(buff, received);
    // Serial.write(buff, received);

    if (remaining > 0)
    {
      remaining -= received;
    }

    // yield();
    // delay(0);

    // Serial.write(buff, received);
    // if (remaining > 0)
    // {
    //   remaining -= received;
    // }

#ifdef DEBUG_AMOR
    Serial.println("espClient status --- " + String(espClient.status()));
    Serial.print(remaining);
    Serial.print("/");
    Serial.println(contentLength);
    printHeap();
#endif

    // yield();
  }

  // for (uint8_t i = 10; i < contentLength; i++)
  // {
  //   /* code */
  //   if (remaining > 0 && espClient.status() == ESTABLISHED)
  //   {
  //     // read up to buffer size
  //   received = espClient.readBytes(buff, ((remaining > sizeof(buff)) ? sizeof(buff) : remaining));

  //   // write it to file
  //   // f.write(buff, received);
  //   Serial.write(buff, received);
  //   Serial.flush();

  //   if (remaining > 0)
  //   {
  //     remaining -= received;
  //   }
  //   // yield();
  //   // delay(0);

  //   // Serial.write(buff, received);
  //   // if (remaining > 0)
  //   // {
  //   //   remaining -= received;
  //   // }

  //   Serial.println("espClient status --- " + String(espClient.status()));
  //   Serial.println(i);
  //   printHeap();
  //   }
  //   else
  //   {
  //     Serial.print("i>");
  //     Serial.println(i);
  //     Serial.print("remaining>");
  //     Serial.println(remaining);
  //     break;
  //   }
  // }

  if (remaining != 0)
  {
#ifdef DEBUG_AMOR
    Serial.println(" Not recieved full data remaing =" + String(remaining) + "/" + String(contentLength));
#endif
  }

  // Close SPIFFS file
  f.close();

  // Stop client
  espClient.stop();

  // return (remaining == 0 ? contentLength : -1);
  if (remaining == 0)
  {
#ifdef DEBUG_AMOR
    Serial.println(contentLength);
#endif
    updatetoConfigJSON("d2fs_finished_flag", "1");
  }
  else
  {
    updatetoConfigJSON("d2fs_finished_flag", "0");
#ifdef DEBUG_AMOR
    Serial.println(F("--1"));
#endif
  }
  // return;

#ifdef DEBUG_AMOR
  Serial.println(F("void download_file_to_fs; END"));
  printHeap();
#endif

  restart_device();
}

void firmware_update_from_fs(String ota_filename)
{
// TODO: to be implemented
#ifdef DEBUG_AMOR
  Serial.println(F("firmware_update_from_fs START"));
  Serial.println(ota_filename);
  printHeap();
#endif

  if (!fsOK)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("An Error has occurred while mounting SPIFFS"));
    printHeap();
#endif
    return;
  }

  if (!ota_filename.startsWith("/"))
  {
    ota_filename = "/" + ota_filename;
  }

  File file = fileSystem->open(ota_filename, "r");
  size_t fileSize = file.size();

  if (!file)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Failed to open file for reading"));
#endif
    return;
  }

  if (!Update.begin(fileSize))
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Cannot do the update"));
#endif
    return;
  };

  Update.writeStream(file);

  if (Update.end())
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Successful update"));
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println("Error Occurred: " + String(Update.getError()));
#endif
    return;
  }

  file.close();

#ifdef DEBUG_AMOR
  Serial.println(F("Reset in 4 seconds..."));
#endif

  delay(4000);

  restart_device();
}

void firmware_update_from_config()
{

#ifdef DEBUG_AMOR
  Serial.println(F("firmware_update_from_config START"));
  printHeap();
#endif

  // espClient.~WiFiClientSecure();
  espClient.stopAll();

#ifdef DEBUG_AMOR
  printHeap();
#endif

  webSocket.~WebSocketsServer();
  clientPubSub.~PubSubClient();
  //
  server.~ESP8266WebServerTemplate();
  webSocket.~WebSocketsServer();

#ifdef DEBUG_AMOR
  Serial.println(F("~PubSubClient"));
  printHeap();
#endif

  // BearSSL::CertStore certStore;

  // String FirmwareVer = {"1.8"};
  // String URL_fw_Version = "/programmer131/otaFiles/master/version.txt";
  // String URL_fw_Bin = "https://raw.githubusercontent.com/programmer131/otaFiles/master/firmware.bin";
  // const char *host = "raw.githubusercontent.com";
  // const int httpsPort = 443;

  // String FirmwareVer = {"1.8"};

  String host_ca = readFromConfigJSON("ota_host_ca");           //ota.der
  String host = readFromConfigJSON("ota_host_url");             // "raw.githubusercontent.com"
  int httpsPort = readFromConfigJSON("ota_host_port").toInt();  // 443
  String URL_fw_Version = readFromConfigJSON("URL_fw_Version"); // without host name
  String URL_fw_Bin = readFromConfigJSON("URL_fw_Bin");

  if (host_ca.length() < 2 ||
      host.length() < 2 ||
      httpsPort < 2 ||
      URL_fw_Version.length() < 2 ||
      URL_fw_Version.length() < 2 ||
      FirmwareVer.length() < 2)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Something is null"));
#endif
  }

#ifdef DEBUG_AMOR
  Serial.print(F("host_ca >"));
  Serial.println(host_ca);
  Serial.print(F("host >"));
  Serial.println(host);
  Serial.print(F("httpsPort >"));
  Serial.println(httpsPort);
  Serial.print(F("URL_fw_Version >"));
  Serial.println(URL_fw_Version);
  Serial.print(F("URL_fw_Bin >"));
  Serial.println(URL_fw_Bin);
  printHeap();
#endif

  // Load private key file
  if (!host_ca.startsWith("/"))
  {
    host_ca = "/" + host_ca;
  }

#ifdef DEBUG_AMOR
  Serial.print(F("host_ca > = "));
  Serial.println(host_ca);

  printHeap();
#endif

  File ota_ca_file = fileSystem->open(host_ca, "r"); //replace private with your uploaded file name
  if (!ota_ca_file)
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Failed to open host_ca cert file"));
    printHeap();
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println(F("Success to open host_ca cert file"));
    printHeap();
#endif
  }

  delay(1000);

  if (espClient.loadCACert(ota_ca_file))
  {
#ifdef DEBUG_AMOR
    Serial.println(F("host_ca cert loaded"));
    printHeap();
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println(F("host_ca cert not loaded"));
    printHeap();
#endif
  }

  ota_ca_file.close();

#ifdef DEBUG_AMOR
  Serial.println(F("ota_ca_file.close();"));
  printHeap();
#endif

  // X509List cert(trustRoot);

  // WiFiClientSecure espClient;

#ifdef DEBUG_AMOR
  printHeap();
#endif

  // espClient.setTrustAnchors(&cert);

  if (!espClient.connect(host, httpsPort))
  {

#ifdef DEBUG_AMOR
    Serial.println(F("Connection to host failed"));
    printHeap();
#endif

    return;
  }

#ifdef DEBUG_AMOR
  printHeap();
#endif

  espClient.print(String("GET ") + URL_fw_Version + " HTTP/1.1\r\n" +
                  "Host: " + host + "\r\n" +
                  "User-Agent: BuildFailureDetectorESP8266\r\n" +
                  "Connection: close\r\n\r\n");

#ifdef DEBUG_AMOR
  printHeap();
#endif

  while (espClient.connected())
  {
#ifdef DEBUG_AMOR
    printHeap();
#endif
    String line = espClient.readStringUntil('\n');
    if (line == "\r")
    {

#ifdef DEBUG_AMOR
      Serial.println(F("Headers received"));
      printHeap();
#endif
      break;
    }
    // yield();
  }

  String payload = espClient.readStringUntil('\n');

#ifdef DEBUG_AMOR
  Serial.println(payload);
  printHeap();
#endif

  payload.trim();

  float currentFW = FirmwareVer.toFloat();
  float gotFW = payload.toFloat();

#ifdef DEBUG_AMOR
  Serial.print(F("currentFW="));
  Serial.println(currentFW);
  Serial.print(F("gotFW="));
  Serial.println(gotFW);
  printHeap();
#endif

  if (gotFW > currentFW)
  {
// UPDATE FW
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("New firmware detected"));
#endif

    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

#ifdef DEBUG_AMOR
    printHeap();
#endif

    t_httpUpdate_return ret = ESPhttpUpdate.update(espClient, URL_fw_Bin);

#ifdef DEBUG_AMOR
    printHeap();
#endif

#ifdef DEBUG_AMOR
    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F("HTTP_UPDATE_NO_UPDATES"));
      break;

    case HTTP_UPDATE_OK:
      Serial.println(F("HTTP_UPDATE_OK"));
      break;
    }
#endif
  }

#ifdef DEBUG_AMOR
  else
  {
    if (currentFW == gotFW)
    {
      Serial.println(F("Device already on latest firmware version"));
    }
    else
    {
      Serial.println(F("Device already on latest firmware version got FW is old"));
    }
  }
#endif

  // if(payload.equals(FirmwareVer))
  // {
  //   Serial.println(F("Device already on latest firmware version"));
  // }
  // else
  // {

  // }
}

// ---- UNIX TIME SETUP END ----

void setupUNIXTime()
{
  timeClient.begin();

#ifdef DEBUG_AMOR
  Serial.println(F("setupUNIXTime"));
#endif
}

// ---- UNIX TIME SETUP END ----

String gethotspotname()
{
#ifdef DEBUG_AMOR
  Serial.print(F("Wifi macAddress> ")); // eg A4:CF:12:C7:E6:AA
#endif

  String mac_add = WiFi.macAddress();

#ifdef DEBUG_AMOR
  Serial.println(mac_add);
#endif

  mac_add.remove(14, 1);
  // Serial.println(mac_add);
  mac_add.remove(11, 1);
  // Serial.println(mac_add);
  mac_add.remove(8, 1);
  // Serial.println(mac_add);
  mac_add.remove(5, 1);
  // Serial.println(mac_add);
  mac_add.remove(2, 1);

  // Serial.println(mac_add);  // eg A4CF12C7E6AA
  String hotspotname = "AmorLamp_"; //TODO:make perfect serial no
  hotspotname.concat(mac_add.substring(6, 12));

#ifdef DEBUG_AMOR
  Serial.print(F("hotspotname> ")); // AmorLamp_C7E6AA
  Serial.println(hotspotname);
#endif

  return hotspotname;
}

//gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager)
{

  // Alternate coloring
  // for (size_t i = 0; i < NUM_LEDS; i++)
  // {
  //   if (i%2 == 0)
  //   {
  //     leds[i] = CRGB::Red;
  //   }
  //   else
  //   {
  //     leds[i] = CRGB::Green;
  //   }
  //   FastLED.show();
  // }

  // 1/2 coloring
  for (size_t i = 0; i < NUM_LEDS; i++)
  {
    if (i < (NUM_LEDS / 2))
    {
      leds[i] = CRGB::Green;
    }
    else
    {
      leds[i] = CRGB::Red;
    }
    FastLED.show();
  }

  // leds[NUM_LEDS - 1] = CRGB::Orange;
  // leds[NUM_LEDS/2] = CRGB::Green;
  // leds[1] = CRGB::Orange;
  FastLED.show();

#ifdef DEBUG_AMOR
  Serial.println(F("Entered config mode"));
#endif
  IPAddress my_softAPIP = WiFi.softAPIP();
#ifdef DEBUG_AMOR
  Serial.println(my_softAPIP);
#endif
  //if you used auto generated SSID, print it
  String my_ssid = myWiFiManager->getConfigPortalSSID();
#ifdef DEBUG_AMOR
  Serial.println(my_ssid);
#endif
  //entered config mode, make led toggle faster
  tickerWifiManagerLed.attach(0.2, tickWifiManagerLed);
  // tickerWifiManagerLed.interval(200);
}

void tickWifiManagerLed()
{
  // toggle state
  // bool state = digitalRead(wifiManagerLED); // get the current state of GPIO1 pin
  // digitalWrite(wifiManagerLED, !state);    // set pin to the opposite state

  digitalWrite(wifiManagerLED, !digitalRead(wifiManagerLED));
  // #ifdef DEBUG_AMOR
  //   Serial.println(F(" ..."));
  //   Serial.println(digitalRead(wifiManagerLED));
  // #endif
}

// wifi managet setup
void wifiManagerSetup()
{
#ifdef DEBUG_AMOR
  printHeap();
  Serial.print(F("WiFi.hostname(); = "));
  Serial.println(WiFi.hostname());
#endif

  leds[NUM_LEDS - 1] = CRGB::Blue;
  leds[1] = CRGB::Blue;
  FastLED.show();
  // Not Required - only for advance testing purpose
  // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  //set led pin as output
  pinMode(wifiManagerLED, OUTPUT);

  // digitalWrite(wifiManagerLED, LOW);
  // start ticker with 0.5 because we start in AP mode and try to connect
  tickerWifiManagerLed.attach(0.6, tickWifiManagerLed);

  // tickerWifiManagerLed.interval(600);
  // tickerWifiManagerLed.update();
  // tickerWifiManagerLed.start();

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  wifiManager.setConfigPortalTimeout(70);
  wifiManager.setConnectTimeout(20);
  wifiManager.setAPNameDeviceId(deviceId.c_str());

  // TODO: no need to uncomment , but check what it does?
  //exit after config instead of connecting
  // wifiManager.setBreakAfterConfig(true);

  //reset settings - for testing , forgets last saved wifi credentials.
  // wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration

#ifdef DEBUG_AMOR
  printHeap();
  Serial.print(F("saved SSIS PSK = "));
  Serial.print(WiFi.SSID());
  Serial.print(F(" "));
  Serial.println(WiFi.psk());
#endif

  if (!wifiManager.autoConnect(gethotspotname().c_str()))
  // if (!wifiManager.autoConnect(deviceId.c_str()))
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("failed to connect and hit timeout"));
#endif
    //reset and try again, or maybe put it to deep sleep
    delay(1000);
    ESP.reset();
    // restart_device(); TODO:Why not this ?
  }

//if you get here you have connected to the WiFi
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println(F("connected...yeey :)"));
#endif
  // printWifiInfo();

  //Stopping WifiConfiguration LED
  // tickerWifiManagerLed.stop();
  tickerWifiManagerLed.detach();

  // to destory tickerWifiManagerLed object
  // tickerWifiManagerLed.~Ticker();  // TODO: test is it required

  //keep LED on
  // digitalWrite(wifiManagerLED, LOW); //Inverted logic of onload leds
  // delay(2000);
  // digitalWrite(wifiManagerLED, HIGH); // TODO: figure out

#ifdef DEBUG_AMOR
  // Serial.println(F("connected...yeey  HIGH"));
  printHeap();
#endif

  // setup internet time to device
  // TODO: if possible put it in setup() directly

  // amorWebsocket_setup();

  // setup_mDNS();
  leds[NUM_LEDS - 1] = CRGB::Yellow;
  leds[1] = CRGB::Yellow;
  FastLED.show();
}

void setup_RGB_leds()
{
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

  leds[NUM_LEDS - 1] = CRGB::Red;
  leds[1] = CRGB::Red;

  FastLED.show();
}

// Interrupt service routine , very light weight
ICACHE_RAM_ATTR void myIRS1()
{
  myISR1_flag = 1;
}

ICACHE_RAM_ATTR void myIRS2()
{
  myISR2_flag = 1;
}

void setupISR1()
{
  // Attach an interrupt to the pin, assign the onChange function as a handler and trigger on changes (LOW or HIGH).
  // attachInterrupt(builtInButton, myIRS1 , FALLING);

  attachInterrupt(digitalPinToInterrupt(builtInButton), myIRS1, FALLING);
}

void setupISR2()
{
  // Attach an interrupt to the pin, assign the onChange function as a handler and trigger on changes (LOW or HIGH).
  // attachInterrupt(builtInButton, myIRS1 , FALLING);

  attachInterrupt(digitalPinToInterrupt(touchInButton), myIRS2, RISING);
}

void setup_ISRs()
{
  setupISR1();
  setupISR2();
}

void printHeap()
{
#ifdef DEBUG_AMOR
  Serial.print(F("Free Heap>"));
  Serial.println(ESP.getFreeHeap());
#endif
}

String list_fs_files_sizes()
{
#ifdef DEBUG_AMOR
  Serial.print(F("Free Flash>"));
  Serial.println(ESP.getFreeSketchSpace());
  printHeap();
#endif

  StaticJsonDocument<512> files_json;

  String str = "f";
  uint8_t counter = 0;

  Dir dir = fileSystem->openDir("/");
  while (dir.next())
  {
    files_json[str + counter] = dir.fileName() + " " + dir.fileSize();
    counter++;
    // yield();
  }
  files_json["count"] = counter;
  files_json["FreeSketchSpace"] = ESP.getFreeSketchSpace();

  // Lastly, you can print the resulting JSON to a String
  String output;
  serializeJson(files_json, output);

#ifdef DEBUG_AMOR
  Serial.print(F("Free Flash>"));
  Serial.println(ESP.getFreeSketchSpace());
  printHeap();
  Serial.println(output);
  serializeJson(files_json, Serial);
#endif

  return output;
}

// TODO:Delte this function in production
#ifdef DEBUG_AMOR
void listAndReadFiles()
{

#ifdef DEBUG_AMOR
  Serial.print(F("Free Flash>"));
  Serial.println(ESP.getFreeSketchSpace());
#endif

  // fileSystem->begin();
  String str = "";
  Dir dir = fileSystem->openDir("/");
  while (dir.next())
  {
    str += dir.fileName();
    str += " / ";
    str += dir.fileSize();
    str += "\r\n";

    if (dir.fileName().startsWith("my"))
    {
#ifdef DEBUG_AMOR
      Serial.println(F("===got my file"));
#endif
      File f = dir.openFile("r");
// Serial.println(f.readString());
#ifdef DEBUG_AMOR
      while (f.available())
      {

        Serial.write(f.read());
      }
#endif
      f.close();

      // for LOGGING
      // File f2 = dir.openFile("a");
      // // Serial.println(f.readString());
      // if (!f2)
      // {
      //   Serial.println(F("Failed to open /stats.txt"));
      //   return;
      // }
      // f2.println(timeClient.getEpochTime());
      // f2.println(timeClient.getFormattedTime());
      // f2.println(ESP.getResetInfo());
      // f2.println("");
      // f2.close();
      // Serial.println(F("LOGS UPDATED"));
    }
    // yield();
  }
#ifdef DEBUG_AMOR
  Serial.print(str);
#endif
  // fileSystem->end();
#ifdef DEBUG_AMOR
  Serial.print(F("Free Flash>"));
  Serial.println(ESP.getFreeSketchSpace());
#endif
}
#endif

String get_ESP_core(String key)
{

  String returnMsg;
  if (key == "getFreeHeap")
  {
    returnMsg = ESP.getFreeHeap();
  }
  else if (key == "getFlashChipSize")
  {
    returnMsg = ESP.getFlashChipSize();
  }
  else if (key == "getFreeContStack")
  {
    returnMsg = ESP.getFreeContStack();
  }
  else if (key == "getFreeSketchSpace")
  {
    returnMsg = ESP.getFreeSketchSpace();
  }
  else if (key == "getHeapFragmentation")
  {
    returnMsg = ESP.getHeapFragmentation();
  }
  else if (key == "getMaxFreeBlockSize")
  {
    returnMsg = ESP.getMaxFreeBlockSize();
  }
  else if (key == "getResetInfo")
  {
    returnMsg = ESP.getResetInfo();
  }
  else if (key == "getResetReason")
  {
    returnMsg = ESP.getResetReason();
  }
  else if (key == "getSketchSize")
  {
    returnMsg = ESP.getSketchSize();
  }
  else if (key == "BSSIDstr")
  {
    returnMsg = WiFi.BSSIDstr();
  }
  else if (key == "hostname")
  {
    returnMsg = WiFi.hostname();
  }
  else if (key == "localIP")
  {

    returnMsg = WiFi.localIP().toString();
  }
  else if (key == "macAddress")
  {
    returnMsg = WiFi.macAddress();
  }
  else if (key == "SSID")
  {
    returnMsg = WiFi.SSID();
  }
  else if (key == "psk")
  {
    returnMsg = WiFi.psk();
  }
  else if (key == "subnetMask")
  {
    returnMsg = WiFi.subnetMask().toString();
  }
  else
  {
    returnMsg = ESP.getFreeHeap();
  }

  return returnMsg;
}

void setup_config_vars()
{
  // read variables from config and update them on setup
#ifdef DEBUG_AMOR
  Serial.println(readFromConfigJSON("device_id"));
  printHeap();
  Serial.println(WiFi.macAddress());
#endif
  deviceId = readFromConfigJSON("device_id");
  if (deviceId.startsWith("ERR"))
  {
    deviceId = "amorAAA" + gethotspotname().substring(8, 15);
  }
  groupId = readFromConfigJSON("groupId");

  readFromConfigJSON("AWS_endpoint").toCharArray(AWS_endpoint, 48);

  x_min_on_value = readFromConfigJSON("x_min_on_value").toInt();
  if (!(x_min_on_value > 0 && x_min_on_value < 61))
  {
    x_min_on_value = 20;
  }
  failed_aws_trials_counter_base = readFromConfigJSON("failed_aws_trials_counter_base").toInt();
  if (!(failed_aws_trials_counter_base > 0 && failed_aws_trials_counter_base < 10))
  {
    failed_aws_trials_counter_base = 4;
  }
  aws_topic_str = "$aws/things/" + deviceId + "/";
  aws_group_topic_str = "amorgroup/" + groupId + "/";

  String myrgbHSL = readFromConfigJSON("myrgbHSL");
  String toSendHSL = readFromConfigJSON("toSendHSL");

  if (myrgbHSL.startsWith("ERR"))
  {
    myrgbHSL = "254255123";
  }

  if (toSendHSL.startsWith("ERR"))
  {
    toSendHSL = "254255123";
  }

  hslS2N(myrgbHSL, 0);
  hslS2N(toSendHSL, 1);

  //   if (WiFi.status() == WL_CONNECTED)
  //   {
  //     if (readFromConfigJSON("localIP") != WiFi.localIP().toString())
  //     {
  //       updatetoConfigJSON("localIP", WiFi.localIP().toString());
  // #ifdef DEBUG_AMOR
  //       Serial.println(" YES IP change is required in flash memory");
  // #endif
  //     }
  //     else
  //     {
  // #ifdef DEBUG_AMOR
  //       Serial.println("No IP change is required in flash memory");
  // #endif
  //     }
  //   }
}

void setup()
{
#ifdef DEBUG_AMOR
  Serial.begin(115200);
  Serial.println(F("==DEBUGGING ENABLED=="));
  Serial.println(WiFi.macAddress());
  Serial.println("amorAAA" + gethotspotname().substring(8, 15));
  printHeap();
  Serial.println(F("fileSystem->begin(); START"));
  Serial.println(F("setup_RGB_leds START"));
#endif

  setup_RGB_leds();

#ifdef DEBUG_AMOR
  Serial.println(F("setup_RGB_leds END"));
  printHeap();
#endif

  fileSystemConfig.setAutoFormat(false);
  fileSystem->setConfig(fileSystemConfig);
  fsOK = fileSystem->begin();

  if (!fsOK)
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println(F("Failed to mount file system"));
#endif
    fileSystem->begin(); // retry once
  }

#ifdef DEBUG_AMOR
  Serial.println(fsOK ? F("Filesystem initialized.") : F("Filesystem init failed!"));
  printHeap();
  Serial.println(F("setup_config_vars START"));
#endif

  setup_config_vars();

#ifdef DEBUG_AMOR
  Serial.println(F("setup_config_vars END"));
  printHeap();
  Serial.println(F("listAndReadFiles START"));
  listAndReadFiles(); // TODO: conmment related code in production.
  Serial.println(F("listAndReadFiles END"));
  printHeap();
  Serial.println(F("readAwsCerts START"));
#endif

  readAwsCerts();

  // fileSystem->end();

#ifdef DEBUG_AMOR
  Serial.println(F("readAwsCerts END ,  fileSystem->end();"));
  printHeap();
  Serial.println(F("setup_ISRs, wifiManagerSetup START"));
#endif

  setup_ISRs();

  // disable_touch_for_x_ms(1200000);

  // calibrate_setup_touch_sensor();

  wifiManagerSetup();
  // digitalWrite(wifiManagerLED, HIGH); // turning off the led after wifi connection

#ifdef DEBUG_AMOR
  Serial.println(F("setup_ISRs, wifiManagerSetup END"));
  printHeap();
  Serial.println(F("void Setup end"));
#endif

#ifdef DEBUG_AMOR
  Serial.println(F("setup_ISRs,setup_RGB_leds, wifiManagerSetup END"));
  printHeap();
  Serial.println(F("setupUNIXTime START"));
#endif

  setupUNIXTime();

#ifdef DEBUG_AMOR
  Serial.println(F("setupUNIXTime END"));
  printHeap();
  Serial.println(F("amorWebsocket_setup START"));
#endif

  websocket_server_mdns_setup();

#ifdef DEBUG_AMOR
  Serial.println(F("amorWebsocket_setup END"));
  printHeap();
  Serial.println(F("setup_mDNS START"));

  Serial.println(F("getBufferSize END"));
  Serial.println(clientPubSub.getBufferSize());
#endif
  int buffer_size = readFromConfigJSON("clientPubSub_buff_size").toInt();
  if (!(buffer_size > 0 && buffer_size < 16384))
  {
    buffer_size = 1024;
  }
  espClient.setBufferSizes(2048, 1024); // TODO: this increased the avaliavle heap from 4k to 17k, but why and how ?

  clientPubSub.setBufferSize(buffer_size);
  // setup_mDNS(); already done above

#ifdef DEBUG_AMOR
  Serial.println(F("getBufferSize END"));
  Serial.println(clientPubSub.getBufferSize());

  Serial.println(F("setup_mDNS END"));
  printHeap();
  Serial.println(F("setup_mDNS START"));
#endif
}

// functions/steps to execute on interrupt 1
void myIRS1_method()
{

#ifdef DEBUG_AMOR
  Serial.println(F("==myIRS1_method called=="));
  printHeap();
#endif

  // set_single_RGB_color((uint8_t)random(256), 255, 255);

  // my_rgb_hsv_values[0] = random(255);
  // turn_on_RGB_led_for_x_mins(10);

  // RGBQueueTask task(MXMINSON, 20);
  // rgb_led_task_queue.push(&task);

  // delay(500);

  // RGBQueueTask task1(MFADEINOUT, 5);
  // rgb_led_task_queue.push(&task1);

  // RGBQueueTask task1(MBLEDX, 5, 1, 0); // 1 to sendHSV , 0 my_HSV
  // rgb_led_task_queue.push(&task1);

  // send_touch_toGroup();

  // #ifdef DEBUG_AMOR
  //   Serial.println(F("groupId>"));
  //   Serial.println(readFromConfigJSON("groupId"));
  //   Serial.println(readFromConfigJSON("toSendHSL"));
  //   Serial.println(readFromConfigJSON("myrgbHSL"));
  // #endif

  // send_touch_toGroup();
  // turn_on_disco_mode_for_x_mins(1);

  // publish_boot_data();
  // forget_saved_wifi_creds(); // TODO: uncomment in prod

  // FirmwareUpdate();
  // printHeap();
  // ticker_test_timer_onoff();
  // printHeap();
  // FirmwareUpdateChaccha();
  // readAndSendFile("config.json");
  // forget_saved_wifi_creds();
  // firmware_update_from_config();
  // String msg = "hii";
  // firmware_update_from_fs(msg);
  // printHeap();
  // readFromConfigJSON("biggestString5");
  // readFromConfigJSON("biggestString4");
  // readFromConfigJSON("biggestString3");
  // readFromConfigJSON("biggestString2");
  // readFromConfigJSON("biggestString1");
  // readFromConfigJSON("biggestString0");
  // printHeap();

  tickWifiManagerLed();
  // download_file_to_fs();
}

void disable_touch_for_x_ms(uint16_t x)
{
  lastValidInterruptTime_2 = millis() + (unsigned long)x;
#ifdef DEBUG_AMOR
  Serial.print(F("disable_touch_for_x_ms > "));
  Serial.println(x);
#endif
}

// functions/steps to execute on interrupt 1
void myIRS2_method()
{
#ifdef DEBUG_AMOR
  Serial.println(F("==myIRS2_method called=="));
  printHeap();
#endif

  myISR2_flag_counter++;
  myISR2_flag_counter_cooldown++;
  if (myISR2_flag_counter_cooldown == 0)
  {
    myISR2_flag_counter_cooldown_millis = millis();
  }

  if (myISR2_flag_counter_cooldown > 15)
  {
    if (millis() - myISR2_flag_counter_cooldown_millis < 8000)
    {
      rgb_led_task_queue.flush();
#ifdef DEBUG_AMOR
      Serial.println(F("==rgb_led_task_queue.flush(); 30 sec mai 30 se jyada touch=="));
      Serial.println(rgb_led_task_queue.getCount());
      Serial.println(rgb_led_task_queue.getRemainingCount());
#endif
      restart_device();
    }
    else
    {
      myISR2_flag_counter_cooldown = 0;
#ifdef DEBUG_AMOR
      Serial.println(F("==rmyISR2_flag_counter_cooldown= 0 RESET=="));
      Serial.println(rgb_led_task_queue.getCount());
      Serial.println(rgb_led_task_queue.getRemainingCount());
#endif
    }
  }

#ifdef DEBUG_AMOR
  Serial.println(F("==myIRS2_method called=="));
  Serial.println(myISR2_flag_counter);
  Serial.println(myISR2_flag_counter_cooldown);
#endif

  send_touch_toGroup();
}

// Restart device after 1s delay
void restart_device()
{
#ifdef DEBUG_AMOR
  Serial.println(F(" !!! RESTARTING ESP !!!"));
#endif
  leds[NUM_LEDS - 1] = CRGB::Red;
  leds[1] = CRGB::Red;
  FastLED.show();
  delay(1000);
  ESP.restart(); //TODO: restart or reset the device ?
}

void forget_saved_wifi_creds()
{

#ifdef DEBUG_AMOR
  Serial.println(ESP.getFreeHeap());
#endif

  WiFiManager wifiManager;
  wifiManager.resetSettings();

#ifdef DEBUG_AMOR
  Serial.println(F(" !!! FORGOT Wifi Id pass :(  !!!"));
  Serial.println(ESP.getFreeHeap());
#endif

  delay(500);
  restart_device();
}

void rgb_led_task_queue_CheckLoop()
{
  if (!rgb_led_task_queue.isEmpty())
  {
    if (!rgb_led_is_busy_flag)
    {
      // PWMQueueTask temp(0,0) = PWMQueueTask(0, 0);
      RGBQueueTask temp(MSKIP, 0); //= PWMQueueTask(0, 0);
      // typedef struct PWMQueueTask PWMQueueTask;
      // struct PWMQueueTask temp;

      //       MSETHSV,    // 1: set_single_RGB_color
      // MFADEINOUT, // 2: tick_fade_in_out_RGB_x_times
      // MXMINSON    // 3: turn_on_RGB_led_for_x_mins

      rgb_led_task_queue.pop(&temp);
      disable_touch_for_x_ms(200);

#ifdef DEBUG_AMOR
      Serial.println(F("length count"));
      Serial.println(rgb_led_task_queue.getCount());
      Serial.println(F("popping up a task:!"));
      Serial.println(temp.argument1);

#endif

      switch (temp.rgbLedMethodCode)
      {
      case MBLEDX:
        blink_led_x_times(temp.argument1, temp.s);
        break;

      case MIRS2:
        myIRS2_method();
        break;

      case MIRS1:
        myIRS1_method();
        break;

      case MUTOSENDRGB:
        update_tosend_rgb_hsv(temp.argument1, temp.s, temp.v);
        break;

      case MUMYRGB:
        update_my_rgb_hsv(temp.argument1, temp.s, temp.v);
        break;
      case MXMINSON:
        /* code */
        turn_on_RGB_led_for_x_mins(temp.argument1);
        break;

      case MFADEINOUT:
        fade_in_out_RGB_x_times(temp.argument1, temp.s);
        break;

      case MSETHSV:
        set_single_RGB_color(temp.argument1, temp.s, temp.v);
        break;

      default:
        break;
      }
    }
  }
}

void timerUpdateLoop()
{
  // tickerWifiManagerLed.update();
  ticker_set_single_RGB_color.update();
  ticker_turn_on_RGB_led_for_x_mins.update();
  ticker_fade_in_out_RGB_x_times.update();
  ticker_blink_led_x_times.update();
  ticker_turn_on_disco_mode_for_x_mins.update();

  // ticker_test_timer.update(); //TODO : remove in production
}

void websocket_server_mdns_loop()
{
  webSocket.loop();
  server.handleClient();
  MDNS.update();
}

// ---- AWS IOT RECONNECT SETUP START ----
// int clientPubSub_connected_counter = 0;
void reconnect_aws()
{
  // printHeap();
  // Loop until we're reconnected

  if (!clientPubSub.connected())
  {
    // clientPubSub_connected_counter++;
    if (millis() - reconnect_aws_millis > 15000)
    {
      // clientPubSub_connected_counter = 0;

      reconnect_aws_millis = millis();
      recon_aws_count++;

      leds[NUM_LEDS - 1] = CRGB::MediumVioletRed;
      leds[1] = CRGB::MediumVioletRed;
      FastLED.show();

      if (recon_aws_count > 60)
      {
        restart_device();
      }

#ifdef DEBUG_AMOR
      // Serial.print(clientPubSub_connected_counter);
      printHeap();
      Serial.print(F("Attempting MQTT connection..."));
      // Serial.print(MQTT_MAX_PACKET_SIZE);
#endif
      // Attempt to connect
      if (clientPubSub.connect(deviceId.c_str()))
      { // update with your own thingName $aws/things/myEspTestWork/shadow/update
#ifdef DEBUG_AMOR
        Serial.println(F("connected"));
        printHeap();
        // Serial.println(F("espClient.flush();"));
        // espClient.flush();
        // printHeap();
        // Serial.println(F("espClient.disableKeepAlive();"));
        // espClient.disableKeepAlive();
        // printHeap();
        // Serial.println(F("espClient.stop();"));
        // espClient.stop();
        // printHeap();
        // Serial.println(F("espClient.stopAll();"));
        // espClient.stopAll();
        // printHeap();

        // TODO : delete ,not for production
        // Once connected, publish an announcement...
        // clientPubSub.publish("outTopic", "hello world");
        // ... and resubscribe
        // clientPubSub.subscribe("inTopic");

        Serial.println(F("subscribeDeviceShadow START"));
#endif

        // subscribeDeviceShadow();

#ifdef DEBUG_AMOR
        Serial.println(F("subscribeDeviceShadow END"));
        printHeap();
        Serial.println(F("setup_config_vars START"));
#endif

        setup_config_vars();

#ifdef DEBUG_AMOR
        Serial.println(F("setup_config_vars END"));
        printHeap();
        Serial.println(F("publish_boot_data START"));
#endif

        // publish_boot_data();

#ifdef DEBUG_AMOR
        Serial.println(F("publish_boot_data END"));
        printHeap();
        Serial.println(F("subscribeDeviceTopics&shadows START"));
#endif

        subscribeDeviceTopics();
        subscribeDeviceShadow();

#ifdef DEBUG_AMOR
        Serial.println(F("subscribeDeviceShadow END"));
        printHeap();
        Serial.println(F("subscribeDeviceShadow START"));
#endif

        getDeviceShadow();

#ifdef DEBUG_AMOR
        Serial.println(F("getDeviceShadow temp end"));
        printHeap();
        Serial.println(F("publish_boot_data temp START"));
#endif

        // subscribeDeviceTopic_group();
        // publish_boot_data();

#ifdef DEBUG_AMOR
        printHeap();
        Serial.println(F("client.subscribe  OK !!!"));
        Serial.println(millis());
#endif
        leds[NUM_LEDS - 1] = CRGB::Green;
        leds[1] = CRGB::Green;

        FastLED.show();
        delay(1000);

        leds[NUM_LEDS - 1] = CRGB::Black;
        leds[1] = CRGB::Black;
        FastLED.show();

        disable_touch_for_x_ms(200);
      }
      else
      {
        failed_aws_trials_counter++;
        if (failed_aws_trials_counter > failed_aws_trials_counter_base)
        {
          //TODO:increase the fail counter count so that user can update its certificates
          // do i need to disable it so that user can update its its end point and certificates
          restart_device();
        }

#ifdef DEBUG_AMOR
        Serial.println(timeClient.getFormattedTime());
        printHeap();
        Serial.print(F("failed, rc="));
        Serial.print(clientPubSub.state());
        Serial.println(F(" try again in 5 seconds"));
        char buf[256];
        espClient.getLastSSLError(buf, 256);
        printHeap();
        Serial.print(F("WiFiClientSecure SSL error: "));
        Serial.println(buf);
#endif
      }
      if (digitalRead(wifiManagerLED) != 1)
      {
        tickWifiManagerLed();
      }
    }
  }
}

// ---- AWS IOT RECONNECT SETUP END ----

// to Keep MQTT aws iot connection alive (i.e. keep PubSubClient alive)
void check_AWS_mqtt()
{

  if (!clientPubSub.connected())
  {
    // listAndReadFiles();
    clientPubSub.disconnect();
    reconnect_aws();
  }
  clientPubSub.loop();
}

// Iterrupt 1 method call check
void myIRS_check()
{
  // Main part of your loop code.
  currrentMillis_interrupt = millis();

  // IRS for in-built button
  if (myISR1_flag)
  {
    if (currrentMillis_interrupt - lastValidInterruptTime_1 > debounceDuration)
    {
      myISR1_flag = 0;
      lastValidInterruptTime_1 = currrentMillis_interrupt;
      myIRS1_method();
    }
    else
    {
      myISR1_flag = 0;
    }
  }

  // IRS for touch sensor module
  if (myISR2_flag)
  {
    if (currrentMillis_interrupt - lastValidInterruptTime_2 > debounceDuration)
    {
      myISR2_flag = 0;
      lastValidInterruptTime_2 = currrentMillis_interrupt;
      myIRS2_method();
    }
    else
    {
      myISR2_flag = 0;
    }
  }
}

void setupUNIXTimeLoop()
{

  bool okTC = timeClient.update();

//   if (!okTC)
//   {

//     timeClient.forceUpdate();
// #ifdef DEBUG_AMOR
//     Serial.println(F("----  timeClient.forceUpdate();  okTC == false ----"));
//     Serial.println(timeClient.getEpochTime());
//     Serial.println(timeClient.getFormattedTime());
// #endif
//   }

  if ((millis() - timeClient_counter_lastvalid_millis > 6000) && okTC)
  {
    timeClient_counter_lastvalid_millis = millis();

#ifdef DEBUG_AMOR
    Serial.println(F("---- 5 sec time update ----"));
    Serial.println(timeClient.getEpochTime());
    Serial.println(timeClient.getFormattedTime());
#endif

    if (!setX509TimeFlag)
    {
      setX509TimeFlag = true;
      espClient.setX509Time(timeClient.getEpochTime());

#ifdef DEBUG_AMOR
      Serial.println(F("espClient.setX509Time(timeClient.getEpochTime());"));
      Serial.println(timeClient.getEpochTime());
      Serial.println(timeClient.getFormattedTime());
#endif
    }
  }
}

void loop()
{
  // check flags is there any interrupt calls made

  myIRS_check();

  setupUNIXTimeLoop();

  check_AWS_mqtt(); //TODO:uncomment

  // server and dns loops
  websocket_server_mdns_loop(); //TODO:uncomment

  rgb_led_task_queue_CheckLoop();

  timerUpdateLoop();

  //this is causing lag in the whole program.
  //TODO:find some other way , force fully turing on onboard led.
  //find why it is  being turned on , is it because of pub sub client?
  // if (digitalRead(wifiManagerLED) != 1)
  // {
  //   tickWifiManagerLed();
  // }
  if (download_file_to_fs_flag)
  {
    download_file_to_fs();
  }

  if (update_shadow_tosend_rgb_hsv_flag)
  {
    update_shadow_tosend_rgb_hsv();
  }
}
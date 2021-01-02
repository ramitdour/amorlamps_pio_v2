#include <Arduino.h>
#include <WiFiManager.h>
#include <FastLED.h>

#ifndef AmorLamps_h
#define AmorLamps_h

#define FASTLED_ESP8266_NODEMCU_PIN_ORDER
// How many leds in your strip?
#define NUM_LEDS 10
// RGB led
#define DATA_PIN 4 //d2




class AmorLamps
{
private:
    bool isLive;

public:
    // Constructor
    AmorLamps(/* args */);
    // Destructor
    ~AmorLamps();

    //Set the bool value of the isLive variable,and return with the newly set value.
    bool setIsLive(bool isLive);
    //Return the bool value of the isLive variable.
    bool getIsLive();
};

void printHeap();

// In void setup

bool updateto_givenfile_ConfigJSON(String &key, String &value, String &filename);
bool updatetoConfigJSON(String key, String value);

String readFrom_given_ConfigJSON(String &key, String &filename);
String readFromConfigJSON(String key);

void setup_config_vars();
void setup_ISRs();
void setupISR1();
void setupISR2();

ICACHE_RAM_ATTR void myIRS1();
ICACHE_RAM_ATTR void myIRS2();

void disable_touch_for_x_ms(uint16_t x);
void setup_RGB_leds();
void calibrate_setup_touch_sensor();

void wifiManagerSetup();
void tickWifiManagerLed();
void configModeCallback(WiFiManager *myWiFiManager);
String gethotspotname();


void setupUNIXTime();
void firmware_update_from_config();
void firmware_update_from_fs(String &ota_filename);
void websocket_server_mdns_setup();

void readAwsCerts();

void subscribeDeviceTopics();

void listAndReadFiles();


// Other methods
void restart_device();
void forget_saved_wifi_creds();

void myIRS1_method();
void myIRS2_method();

void reconnect_aws();
void aws_callback(char *topic, byte *payload, unsigned int length);
void rpc_method_handler(byte *payload, unsigned int length);


enum methodCode
{
  MSKIP,
  MSETHSV,     // 1: set_single_RGB_color
  MFADEINOUT,  // 2: fade_in_out_RGB_x_times
  MXMINSON,    // 3: turn_on_RGB_led_for_x_mins
  MUMYRGB,     // 4: update_my_rgb_hsv
  MUTOSENDRGB, // 5: update_tosend_rgb_hsv
  MIRS1,       // 6: myIRS1_method();
  MIRS2,       // 7: myIRS2_method();
  MBLEDX       // 8: blink_led_x_times

};

void method_handler(methodCode mc, int args, bool plus1arg, uint8_t s, uint8_t v);

void tick_set_single_RGB_color();
void tick_turn_on_RGB_led_for_x_mins();
void tick_fade_in_out_RGB_x_times();
void tick_blink_led_x_times();
void tick_turn_on_disco_mode_for_x_mins();
// void tickWifiManagerLed();

void turn_off_disco_mode();
void turn_on_disco_mode_for_x_mins(int x);
void set_single_RGB_color(uint8_t h, uint8_t s, uint8_t v);
void turn_on_RGB_led_for_x_mins(int x);
void fade_in_out_RGB_x_times(int x);
void blink_led_x_times(int x, bool toSendHsv);

void hslS2N(String mystr, uint8_t v);
String hslN2S(uint8_t h, uint8_t s, uint8_t l);


// In void loop
void myIRS_check();


void setupUNIXTimeLoop();
void check_AWS_mqtt();
void websocket_server_mdns_loop();

#endif
/* ================= Import libraries START AVISH================= */

#include <littlefs.h>          // Although littlefs is inported so FS is not required but , in my case I had to add FS
#include <Ticker.h>            //for LED status if Wifi
#include <WiFiManager.h>       //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266WiFi.h>       // For basic functionality of ESP8266
#include <ESP8266HTTPClient.h> // For HTTP client functionality of ESP8266
// Fast led for RGB leds
#include <FastLED.h>

#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"

// <AWS IOT + MQTT - PUB/SUB>

#include <PubSubClient.h> // For ESP8266 to publish/subscribe messaging with aws MQTT server
#include <NTPClient.h>    // To connect Network Time Protocol (NTP) server for epoch time
#include <WiFiUdp.h>      // Implementations send and receive timestamps using the User Datagram Protocol (UDP)

#include <ArduinoJson.h> // Serial data <-> JSON data , for conversions

// <For PWM led task queue>
#include <cppQueue.h> // For PWM led task queue

#include "amorlamps.h"

// HTTP_HEAD

/* ================= Import libraries END ================= */

// # define Serial.printf "Serial.println"
const String FirmwareVer = {"1.0"};

#define DEBUG_AMOR 1; // TODO:comment in production

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

// Define the array of leds
CRGB leds[NUM_LEDS];

// WIFI MANAGER
// WiFiManager wifiManager; //Soft wifi configuration

const uint8_t wifiManagerLED = 2; // TODO: Upadte as per the requirements

// Ticker tickerWifiManagerLed(tickWifiManagerLed, 1000, 0, MILLIS);
Ticker tickerWifiManagerLed;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");           // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)

unsigned long wsCleanupMillis = 0;

const char *http_username = "admin";
const char *http_password = "admin";

//flag to use from web update to reboot the ESP
bool shouldReboot = false;

String deviceId = "amorAAA_C7ED21";
String groupId = "123456";

// AWS , MQTT , PUB/SUB + internet Time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
bool setX509TimeFlag = false;

unsigned long timeClient_counter_lastvalid_millis = 0;
// uint8_t timeClient_counter = 0;

// const char *AWS_endpoint = "a2q0zes51fm6yg-ats.iot.us-west-2.amazonaws.com"; //MQTT broker ip
const char *AWS_endpoint = "a3an4l5rg1sm5p-ats.iot.ap-south-1.amazonaws.com"; //MQTT broker ip
uint8_t failed_aws_trials_counter = 0;
unsigned long reconnect_aws_millis = 0;
// const char *aws_topic = "$aws/things/esp8266_C7ED21/";

// aws_topic;

String aws_topic_str = "$aws/things/" + deviceId + "/";
// char *aws_topic = "$aws/things/esp8266_C7ED21/";
String aws_group_topic_str = "amorgroup/" + groupId + "/";

void aws_callback(char *topic, byte *payload, unsigned int length);

WiFiClientSecure espClient;

PubSubClient clientPubSub(AWS_endpoint, 8883, aws_callback, espClient); //set MQTT port number to 8883 as per //standard

long lastMsg = 0;
char msg[50];
int value = 0;

void aws_callback(char *topic, byte *payload, unsigned int length)
{
#ifdef DEBUG_AMOR
  Serial.println("aws_callback");
  printHeap();
#endif
}

// void subscribeDeviceTopics()
// {

// #ifdef DEBUG_AMOR
//   Serial.println("subscribeDeviceTopics before");
//   Serial.println(aws_topic_str);
//   Serial.println(aws_group_topic_str);
// #endif

//   clientPubSub.subscribe((aws_topic_str + "+").c_str());
//   clientPubSub.subscribe((aws_group_topic_str + "+").c_str());

//   //Sending ip wo aws servers for debugging

//   send_responseToAWS(deviceId + " = " + readFromConfigJSON("localIP"));

// #ifdef DEBUG_AMOR
//   Serial.println("subscribeDeviceTopics DONEE ...");
//   Serial.println(ESP.getFreeHeap());
// #endif
// }

// ---- CERTIFICATES READ for  AWS IOT SETUP START ----

void readAwsCerts()
{
#ifdef DEBUG_AMOR
  Serial.print("Heap: ");
  Serial.println(ESP.getFreeHeap());
  printHeap();
#endif

  leds[NUM_LEDS - 1] = CRGB::Violet;
  FastLED.show();
  // TODO:verify that weather closing the opened file later on makes any difference
  if (!LittleFS.begin())
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("Failed to mount file system");
#endif
    return;
  }

  // Load certificate file
  File cert = LittleFS.open("/cert.der", "r"); //replace cert.crt with your uploaded file name
  if (!cert)
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("Failed to open cert file");
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("Success to open cert file");
#endif
  }

  delay(1000);

  if (espClient.loadCertificate(cert))
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("cert loaded");
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("cert not loaded");
#endif
  }

  cert.close();

  // Load private key file
  File private_key = LittleFS.open("/private.der", "r"); //replace private with your uploaded file name
  if (!private_key)
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("Failed to open private cert file");
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("Success to open private cert file");
#endif
  }

  delay(1000);

  if (espClient.loadPrivateKey(private_key))
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("private key loaded");
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("private key not loaded");
#endif
  }

  private_key.close();

  // Load CA file
  File ca = LittleFS.open("/ca.der", "r"); //replace ca with your uploaded file name
  if (!ca)
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("Failed to open ca ");
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("Success to open ca");
#endif
  }

  delay(1000);

  if (espClient.loadCACert(ca))
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("ca loaded");
#endif
  }

  else
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("ca failed");
#endif
  }

  ca.close();

#ifdef DEBUG_AMOR
  printHeap();
#endif

  LittleFS.end();

#ifdef DEBUG_AMOR
  printHeap();
#endif
}

void handleFileUpload()
{ // upload a new file to the SPIFFS
}

void onRequest(AsyncWebServerRequest *request)
{
  //Handle Unknown Request
  request->send(404);
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
//Handle body
#ifdef DEBUG_AMOR
  Serial.println("Handle body...");
#endif
}

// #include <SPIFFSEditor.h>
// SPIFFSEditor editor("","",LittleFS);

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  //Handle upload
#ifdef DEBUG_AMOR
  Serial.println("onUpload...");
  Serial.print(index);
  Serial.print(" ");
  Serial.print(len);
  Serial.print(" ");
  Serial.print(final);
  Serial.print(" ");
#endif

  // request->send(LittleFS, filename, String(), true);
  // AsyncWebServerResponse *response = request->beginResponse(LittleFS, filename, String(), true);
  // response->addHeader("Server", "ESP Async Web Server");
  // request->send(response);

  if (!index)
  {
#ifdef DEBUG_AMOR
    Serial.print("Before Free Flash>");
    Serial.println(ESP.getFreeSketchSpace());
#endif
    request->_tempFile = LittleFS.open(filename, "w");
  }
  if (request->_tempFile)
  {
    if (len)
    {
      request->_tempFile.write(data, len);
#ifdef DEBUG_AMOR
      printHeap();
#endif
    }
    if (final)
    {
      request->_tempFile.close();
#ifdef DEBUG_AMOR
      Serial.print("After Free Flash>");
      Serial.println(ESP.getFreeSketchSpace());
#endif
    }
  }
  if (final)
  {
#ifdef DEBUG_AMOR
    Serial.printf("Update Success: %uB\n", index + len);
#endif
  }
}

void onUpload2(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
//Handle upload
#ifdef DEBUG_AMOR
  Serial.println("onUpload...");
#endif

  if (!index)
  {
#ifdef DEBUG_AMOR
    Serial.printf("Got file: %s\n", filename.c_str());
#endif
  }

  // if (true)
  // {
  //   AsyncWebServerResponse *response = request->beginResponse(LittleFS, filename, String(), true);
  //   response->addHeader("Server", "ESP Async Web Server");
  //   request->send(response);
  //    }

  Serial.println(" +++request->headers()");
  for (size_t i = 0; i < request->headers(); i++)
  {
    Serial.println(request->getHeader(i)->name());
    Serial.println(request->getHeader(i)->value());
    Serial.println("---");
  }
  Serial.println(" ++++ request->params()");

  for (size_t i = 0; i < request->params(); i++)
  {
    Serial.println(request->getParam(i)->name()); // listHeaders(request);
    Serial.println(request->getParam(i)->value());
    Serial.println("---");
  }
  Serial.println(" ++++ request->args()");
  for (size_t i = 0; i < request->args(); i++)
  {
    Serial.println(request->arg(i)); // listHeaders(request);
    Serial.println(request->argName(i));
    Serial.println(request->pathArg(i));
    // Serial.println(request->getParam(i)->value());
    Serial.println("---");
  }

  if (request->hasParam("filename", true))
  { // Download file
    if (request->hasArg("fsupload"))
    { // file download
      Serial.println("Download Filename: " + request->arg("filename"));
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, request->arg("filename"), String(), true);
      response->addHeader("Server", "ESP Async Web Server");
      request->send(response);
      return;
    }
    else if (request->hasArg("delete"))
    { // Delete file
      LittleFS.remove(request->getParam("filename", true)->value());
      // request->send(200, "", "DELETE: "+request->getParam("path", true)->value());
      request->redirect("/files");
    }
    else
    {
      Serial.println("SOMETHING IS WRING");
    }
  }
  else if (request->hasArg("goBack"))
  { // GO Back Button
    request->redirect("register");
  }

  if (final)
  {
#ifdef DEBUG_AMOR
    Serial.printf("Update Success: %uB\n", index + len);
#endif
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
//Handle WebSocket event
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println("WebSocket...");
  Serial.println(type);
  // Serial.print(*data);
  Serial.println(len);
#endif

  if (type == WS_EVT_CONNECT)
  {
    ws.cleanupClients(); // got new WS client forget all old ones !
    //client connected
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    //client disconnected
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
    ws.cleanupClients();
  }
  else if (type == WS_EVT_ERROR)
  {
    //error was received from the other end
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    //pong message was received (in response to a ping request maybe)
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
  else if (type == WS_EVT_DATA)
  {
    //data packet
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len)
    {
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
      if (info->opcode == WS_TEXT)
      {
        data[len] = 0;
        Serial.printf("%s\n", (char *)data);
      }
      else
      {
        for (size_t i = 0; i < info->len; i++)
        {
          Serial.printf("%02x ", data[i]);
        }
        Serial.printf("\n");
      }

      if (info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    }
    else
    {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0)
      {
        if (info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);
      if (info->message_opcode == WS_TEXT)
      {
        data[len] = 0;
        Serial.printf("%s\n", (char *)data);
      }
      else
      {
        for (size_t i = 0; i < len; i++)
        {
          Serial.printf("%02x ", data[i]);
        }
        Serial.printf("\n");
      }

      if ((info->index + len) == info->len)
      {
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final)
        {
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}

void setup_async()
{
#ifdef DEBUG_AMOR
  Serial.println("setup_async...");
#endif
  LittleFS.begin();
  // attach AsyncWebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  // attach AsyncEventSource
  server.addHandler(&events);

  // respond to GET requests on URL /heap
  server.on("/heap", HTTP_GET_ASYNC, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  // upload a file to /upload
  server.on(
      "/upload", HTTP_POST_ASYNC, [](AsyncWebServerRequest *request) {
        request->send(200);
      },
      onUpload);

  // send a file when /index is requested
  //   server.on("/index", HTTP_ANY_ASYNC, [](AsyncWebServerRequest *request) {
  // #ifdef DEBUG_AMOR
  //     Serial.println("fetching index file");
  //     Serial.println();
  // #endif
  //     request->send(LittleFS, "/index.html");
  //   });

  // HTTP basic authentication
  server.on("/login", HTTP_GET_ASYNC, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send(200, "text/plain", "Login Success!");
  });

  // Simple Firmware Update Form
  server.on("/update", HTTP_GET_ASYNC, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
  });

  server.on(
      "/update", HTTP_POST_ASYNC, [](AsyncWebServerRequest *request) {
    shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot?"OK":"FAIL");
    response->addHeader("Connection", "close");
    request->send(response); }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if(!index){
#ifdef DEBUG_AMOR
      Serial.printf("Update Start: %s\n", filename.c_str());
#endif
      Update.runAsync(true);
      if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
#ifdef DEBUG_AMOR
        Update.printError(Serial);
#endif
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len){
#ifdef DEBUG_AMOR
        Update.printError(Serial);
#endif
      }
    }
    if(final){
      if(Update.end(true)){
#ifdef DEBUG_AMOR
        Serial.printf("Update Success: %uB\n", index+len);
#endif
      } else {
        Update.printError(Serial);
      }
    } });

  // attach filesystem root at URL /fs
  // server.serveStatic("/fs", LittleFS, "/");

  server.serveStatic("/index", LittleFS, "/index.html");
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/script.js", LittleFS, "/script.js");
  server.serveStatic("/mytxt", LittleFS, "/my.txt.txt");

  // Simple File Update/Upload Form
  server.on("/fsupload", HTTP_GET_ASYNC, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<form name='myUploadForm' id='myUploadForm' method='POST' action='/ fsupload' enctype='multipart/form-data'><div><label for='filename'>Select file</label><input name='filename' id='filename' type='file'></div><div><label for='fsupload'>PerformUpload</label><input name='fsupload' id='fsupload' type='submit' value='Update'></div></form>");
    //<div><label for='say'>What greeting do you want to say?</label><input name='say' id='say' value='Hi'></div><div><label for='to'>Who do you want to say it to?</label><input name='to' id='to' value='Mom'></div>
  });

  server.on(
      "/fsupload", HTTP_POST_ASYNC, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", "200 ok ");
      },
      onUpload);

  // Catch-All Handlers
  // Any request that can not find a Handler that canHandle it
  // ends in the callbacks below.
  server.onNotFound(onRequest);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);

  server.begin();
}

void loop_async()
{
  if (shouldReboot)
  {
#ifdef DEBUG_AMOR
    Serial.println("Rebooting...");
#endif
    delay(100);
    ESP.restart();
  }

  // static char temp[128];
  // sprintf(temp, "Seconds since boot: %u", millis() / 1000);
  // events.send(temp, "time"); //send event "time"
}

String readFromConfigJSON(String key)
{
  // TODO: check is little fs is begun before this call ?
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile)
  {
#ifdef DEBUG_AMOR
    Serial.println("Failed to open config file");
#endif
    // return (String) false;
    return "ERR-FO";
  }

  size_t size = configFile.size();

#ifdef DEBUG_AMOR
  Serial.print("Config file size=");
  Serial.println(size);
#endif

  // totalSize = totalSize + size;

  // Serial.print("TOTAL file size FINAL !!! =");
  // Serial.println(totalSize);

  if (size > 1024 * 3)
  {
#ifdef DEBUG_AMOR
    Serial.println("Config file size is too large");
#endif
    // return (String) false;
    return "ERR-FSL";
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonDocument<1300> doc;

  auto error = deserializeJson(doc, buf.get());

// printing json data to Serial Port in pretty format
#ifdef DEBUG_AMOR
  Serial.println("");
  // serializeJsonPretty(doc, Serial);
  Serial.println("");
#endif

  if (error)
  {
#ifdef DEBUG_AMOR
    Serial.println("Failed to parse config file");
#endif
    //return false;
    // return (String)value;
    return "ERR-FPF";
  }

  const char *value = doc[key];
  // String value = doc[key]; // This didn't worked out

  // Real world application would store these values in some variables for
  // later use.

#ifdef DEBUG_AMOR
  Serial.print("Loaded ");
  Serial.print(key);
  Serial.print(":");
  Serial.println(value);
#endif

  // return true;
  return value;
}

// ---- UNIX TIME SETUP END ----
void setupUNIXTime()
{
  timeClient.begin();

#ifdef DEBUG_AMOR
  Serial.println("setupUNIXTime");
#endif
}

void setupUNIXTimeLoop()
{

  bool okTC = timeClient.update();

  if ((millis() - timeClient_counter_lastvalid_millis > 6000) && okTC)
  {
    timeClient_counter_lastvalid_millis = millis();

    // #ifdef DEBUG_AMOR
    //     Serial.println("---- 5 sec time update ----");
    //     Serial.println(timeClient.getEpochTime());
    //     Serial.println(timeClient.getFormattedTime());
    // #endif

    if (!setX509TimeFlag)
    {
      setX509TimeFlag = true;
      espClient.setX509Time(timeClient.getEpochTime());

#ifdef DEBUG_AMOR
      Serial.println("espClient.setX509Time(timeClient.getEpochTime());");
      Serial.println(timeClient.getEpochTime());
      Serial.println(timeClient.getFormattedTime());
#endif
    }
  }
}

// ---- UNIX TIME SETUP END ----

String gethotspotname()
{
#ifdef DEBUG_AMOR
  Serial.print("Wifi macAddress> "); // eg A4:CF:12:C7:E6:AA
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
  Serial.print("hotspotname> "); // esp8266_C7E6AA
  Serial.println(hotspotname);
#endif

  return hotspotname;
}

//gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager)
{
#ifdef DEBUG_AMOR
  Serial.println("Entered config mode");
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
#ifdef DEBUG_AMOR
  Serial.println(" ...");
#endif
}

// wifi managet setup
void wifiManagerSetup()
{
#ifdef DEBUG_AMOR
  printHeap();
  Serial.print("WiFi.hostname(); = ");
  Serial.println(WiFi.hostname());
#endif

  leds[NUM_LEDS - 1] = CRGB::Blue;
  FastLED.show();
  // Not Required - only for advance testing purpose
  // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  //set led pin as output
  pinMode(wifiManagerLED, OUTPUT);

  digitalWrite(wifiManagerLED, LOW);
  // start ticker with 0.5 because we start in AP mode and try to connect
  tickerWifiManagerLed.attach(0.6, tickWifiManagerLed);

  // tickerWifiManagerLed.interval(600);
  // tickerWifiManagerLed.update();
  // tickerWifiManagerLed.start();

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  wifiManager.setConfigPortalTimeout(40);
  wifiManager.setConnectTimeout(20);

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
  Serial.print("saved SSIS PSK = ");
  Serial.print(WiFi.SSID());
  Serial.print(" ");
  Serial.println(WiFi.psk());
#endif

  if (!wifiManager.autoConnect(gethotspotname().c_str()))
  // if (!wifiManager.autoConnect(deviceId.c_str()))
  {
#ifdef DEBUG_AMOR
    printHeap();
    Serial.println("failed to connect and hit timeout");
#endif
    //reset and try again, or maybe put it to deep sleep
    delay(1000);
    ESP.reset();
  }

//if you get here you have connected to the WiFi
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println("connected...yeey :)");
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
  // Serial.println("connected...yeey  HIGH");
  printHeap();
#endif

  // setup internet time to device
  // TODO: if possible put it in setup() directly
  setupUNIXTime();

  // amorWebsocket_setup();

  // setup_mDNS();
  leds[NUM_LEDS - 1] = CRGB::Yellow;
  FastLED.show();
}

void setup_RGB_leds()
{
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

  leds[NUM_LEDS - 1] = CRGB::Red;

  FastLED.show();
}

// Interrupt service routine , very light weight
ICACHE_RAM_ATTR void myIRS1()
{

  if (currrentMillis_interrupt - lastValidInterruptTime_1 > debounceDuration)
  {
    myISR1_flag = 1;
  }
}

ICACHE_RAM_ATTR void myIRS2()
{

  if (currrentMillis_interrupt - lastValidInterruptTime_2 > debounceDuration)
  {
    myISR2_flag = 1;
  }
}

void setupISR1()
{
  currrentMillis_interrupt = millis();

  // Attach an interrupt to the pin, assign the onChange function as a handler and trigger on changes (LOW or HIGH).
  // attachInterrupt(builtInButton, myIRS1 , FALLING);

  attachInterrupt(digitalPinToInterrupt(builtInButton), myIRS1, FALLING);
}

void setupISR2()
{
  currrentMillis_interrupt = millis();

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
  Serial.print("Free Heap>");
  Serial.println(ESP.getFreeHeap());
#endif
}

// TODO:Delte this function in production
void listAndReadFiles()
{

#ifdef DEBUG_AMOR
  Serial.print("Free Flash>");
  Serial.println(ESP.getFreeSketchSpace());
#endif

  LittleFS.begin();
  String str = "";
  Dir dir = LittleFS.openDir("/");
  while (dir.next())
  {
    str += dir.fileName();
    str += " / ";
    str += dir.fileSize();
    str += "\r\n";

    if (dir.fileName().startsWith("my"))
    {
      Serial.println("===got my file");
      File f = dir.openFile("r");
      // Serial.println(f.readString());
      while (f.available())
      {
        Serial.write(f.read());
      }
      f.close();
    }
  }
  Serial.print(str);
  LittleFS.end();
#ifdef DEBUG_AMOR
  Serial.print("Free Flash>");
  Serial.println(ESP.getFreeSketchSpace());
#endif
}

void setup_config_vars()
{
  // read variables from config and update them on setup

#ifdef DEBUG_AMOR

  Serial.println(readFromConfigJSON("device_id"));
  printHeap();
#endif
}

void setup()
{
#ifdef DEBUG_AMOR
  Serial.begin(115200);
  Serial.println("==DEBUGGING ENABLED==");
  printHeap();
  Serial.println("LittleFS.begin(); START");
#endif

  LittleFS.begin();

#ifdef DEBUG_AMOR
  printHeap();
  Serial.println("setup_config_vars START");
#endif

  setup_config_vars();

#ifdef DEBUG_AMOR
  Serial.println("setup_config_vars END");
  printHeap();
  Serial.println("listAndReadFiles START");
#endif

  listAndReadFiles();

#ifdef DEBUG_AMOR
  Serial.println("listAndReadFiles END");
  printHeap();
  Serial.println("readAwsCerts START");
#endif

  readAwsCerts();

  LittleFS.end();

#ifdef DEBUG_AMOR
  Serial.println("readAwsCerts END ,  LittleFS.end();");
  printHeap();
  Serial.println("setup_ISRs,setup_RGB_leds, wifiManagerSetup START");
#endif

  setup_ISRs();

  // disable_touch_for_x_ms(1200000);

  setup_RGB_leds();
  // calibrate_setup_touch_sensor();

  wifiManagerSetup();
  digitalWrite(wifiManagerLED, HIGH); // turning off the led after wifi connection

#ifdef DEBUG_AMOR
  Serial.println("setup_ISRs,setup_RGB_leds, wifiManagerSetup END");
  printHeap();
  Serial.println("setup_async START");
#endif

  setup_async();

#ifdef DEBUG_AMOR
  Serial.println("setup_async END");
  printHeap();
  Serial.println("void Setup end");
#endif
}

// this will close connectin with unused/old websockets clients
void wsCleanup()
{
  if (millis() - wsCleanupMillis > 2000)
  {
    wsCleanupMillis = millis();
    ws.cleanupClients();

#ifdef DEBUG_AMOR

    Serial.print("wsC");
    printHeap();
#endif
  }
}

// functions/steps to execute on interrupt 1
void myIRS1_method()
{

#ifdef DEBUG_AMOR
  Serial.println("==myIRS1_method called==");
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
  //   Serial.println("groupId>");
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
  forget_saved_wifi_creds();
}

void disable_touch_for_x_ms(uint16_t x)
{
  lastValidInterruptTime_2 = millis() + (unsigned long)x;
#ifdef DEBUG_AMOR
  Serial.print("disable_touch_for_x_ms > ");
  Serial.println(x);
#endif
}

// functions/steps to execute on interrupt 1
void myIRS2_method()
{
#ifdef DEBUG_AMOR
  Serial.println("==myIRS2_method called==");
  printHeap();
#endif

  //   myISR2_flag_counter++;
  //   myISR2_flag_counter_cooldown++;
  //   if (myISR2_flag_counter_cooldown == 0)
  //   {
  //     myISR2_flag_counter_cooldown_millis = millis();
  //   }

  //   if (myISR2_flag_counter_cooldown > 15)
  //   {
  //     if (millis() - myISR2_flag_counter_cooldown_millis < 15000)
  //     {
  //       rgb_led_task_queue.flush();
  // #ifdef DEBUG_AMOR
  //       Serial.println("==rgb_led_task_queue.flush(); 30 sec mai 30 se jyada touch==");
  //       Serial.println(rgb_led_task_queue.getCount());
  //       Serial.println(rgb_led_task_queue.getRemainingCount());
  // #endif
  //       restart_device();
  //     }
  //     else
  //     {
  //       myISR2_flag_counter_cooldown = 0;
  // #ifdef DEBUG_AMOR
  //       Serial.println("==rmyISR2_flag_counter_cooldown= 0 RESET==");
  //       Serial.println(rgb_led_task_queue.getCount());
  //       Serial.println(rgb_led_task_queue.getRemainingCount());
  // #endif
  //     }
  //   }

  // #ifdef DEBUG_AMOR
  //   Serial.println("==myIRS2_method called==");
  //   Serial.println(myISR2_flag_counter);
  //   Serial.println(myISR2_flag_counter_cooldown);
  // #endif

  // send_touch_toGroup();
}

// Restart device after 1s delay
void restart_device()
{
#ifdef DEBUG_AMOR
  Serial.println(" !!! RESTARTING ESP !!!");
#endif
  leds[NUM_LEDS - 1] = CRGB::Red;
  FastLED.show();
  delay(1000);
  ESP.restart();
}

void forget_saved_wifi_creds()
{

#ifdef DEBUG_AMOR
  Serial.println(ESP.getFreeHeap());
#endif

  WiFiManager wifiManager;
  wifiManager.resetSettings();

#ifdef DEBUG_AMOR
  Serial.println(ESP.getFreeHeap());
#endif

  delay(500);
  ESP.restart();
}

// ---- AWS IOT RECONNECT SETUP START ----
int clientPubSub_connected_counter = 0;
void reconnect_aws()
{
  // printHeap();
  // Loop until we're reconnected

  if (!clientPubSub.connected())
  {
    clientPubSub_connected_counter++;
    if (millis() - reconnect_aws_millis > 5000)
    { clientPubSub_connected_counter = 0;

      reconnect_aws_millis = millis();

      leds[NUM_LEDS - 1] = CRGB::MediumVioletRed;
      FastLED.show();

#ifdef DEBUG_AMOR
      Serial.print(clientPubSub_connected_counter);
      printHeap();
      Serial.print("Attempting MQTT connection...");
      // Serial.print(MQTT_MAX_PACKET_SIZE);
#endif
      // Attempt to connect
      if (clientPubSub.connect(deviceId.c_str()))
      { // update with your own thingName $aws/things/myEspTestWork/shadow/update
#ifdef DEBUG_AMOR
        Serial.println("connected");
        printHeap();
        Serial.println("espClient.flush();");
        espClient.flush();
        printHeap();
        // Serial.println("espClient.disableKeepAlive();");
        // espClient.disableKeepAlive();
        // printHeap();
        // Serial.println("espClient.stop();");
        // espClient.stop();
        // printHeap();
        // Serial.println("espClient.stopAll();");
        // espClient.stopAll();
        // printHeap();

#endif
        // Once connected, publish an announcement...
        clientPubSub.publish("outTopic", "hello world");
        // ... and resubscribe
        clientPubSub.subscribe("inTopic");

        // subscribeDeviceShadow();
        // printHeap();
        // setup_config_vars();
        // printHeap();
        // publish_boot_data();
        // printHeap();
        // subscribeDeviceTopics();
        // printHeap();

#ifdef DEBUG_AMOR
        printHeap();
        Serial.println("client.subscribe  OK !!!");
#endif

        leds[NUM_LEDS - 1] = CRGB::Green;
        FastLED.show();
        delay(1000);

        leds[NUM_LEDS - 1] = CRGB::Black;
        FastLED.show();

        disable_touch_for_x_ms(200);
      }
      else
      {
        failed_aws_trials_counter++;
        if (failed_aws_trials_counter > 4)
        {
          restart_device();
        }

#ifdef DEBUG_AMOR
        printHeap();
        Serial.print("failed, rc=");
        Serial.print(clientPubSub.state());
        Serial.println(" try again in 5 seconds");
#endif

        char buf[256];
        espClient.getLastSSLError(buf, 256);

#ifdef DEBUG_AMOR
        printHeap();
        Serial.print("WiFiClientSecure SSL error: ");
        Serial.println(buf);
#endif
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
    myISR1_flag = 0;
    lastValidInterruptTime_1 = currrentMillis_interrupt;
    myIRS1_method();
  }

  // IRS for touch sensor module
  if (myISR2_flag)
  {
    myISR2_flag = 0;
    lastValidInterruptTime_2 = currrentMillis_interrupt;

    myIRS2_method();
  }
}

void loop()
{
  // check flags is there any interrupt calls made
  myIRS_check();

  loop_async();

  wsCleanup();

  setupUNIXTimeLoop();

  check_AWS_mqtt();
}
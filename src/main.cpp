/* ================= Import libraries START ================= */

#include <littlefs.h>    // Although littlefs is inported so FS is not required but , in my case I had to add FS
#include <Ticker.h>      //for LED status if Wifi
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266WiFi.h> // For basic functionality of ESP8266
// Fast led for RGB leds
#include <FastLED.h>

#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"

#include "amorlamps.h"

// HTTP_HEAD

/* ================= Import libraries END ================= */

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

const char *http_username = "admin";
const char *http_password = "admin";

//flag to use from web update to reboot the ESP
bool shouldReboot = false;



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
    Serial.print("Free Flash>");
    Serial.println(ESP.getFreeSketchSpace());
#endif
    request->_tempFile = LittleFS.open(filename, "w");
  }
  if (request->_tempFile)
  {
    if (len)
    {
      request->_tempFile.write(data, len);
    }
    if (final)
    {
      request->_tempFile.close();
#ifdef DEBUG_AMOR
      Serial.print("Free Flash>");
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
  Serial.println("WebSocket...");
#endif
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
  server.on("/index", HTTP_ANY_ASYNC, [](AsyncWebServerRequest *request) {
#ifdef DEBUG_AMOR
    Serial.println("fetching index file");
    Serial.println();
#endif
    request->send(LittleFS, "/index.html");
  });

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

  // server.serveStatic("/index", LittleFS, "/index.html");
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/script.js", LittleFS, "/script.js");

  // Simple File Update/Upload Form
  server.on("/fsupload", HTTP_GET_ASYNC, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<form name='myUploadForm' id='myUploadForm' method='POST' action='/ fsupload' enctype='multipart/form-data'><div><label for='filename'>Select file</label><input name='filename' id='filename' type='file'></div><div><label for='fsupload'>PerformUpload</label><input name='fsupload' id='fsupload' type='submit' value='Update'></div></form>");
    //<div><label for='say'>What greeting do you want to say?</label><input name='say' id='say' value='Hi'></div><div><label for='to'>Who do you want to say it to?</label><input name='to' id='to' value='Mom'></div>
  });

  server.on(
      "/fsupload", HTTP_POST_ASYNC, [](AsyncWebServerRequest *request) {
        request->send(200);
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
  static char temp[128];
  sprintf(temp, "Seconds since boot: %u", millis() / 1000);
  events.send(temp, "time"); //send event "time"
}

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
  // setupUNIXTime();

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

void disable_touch_for_x_ms(uint x)
{
  lastValidInterruptTime_2 = millis() + (unsigned long)x;
#ifdef DEBUG_AMOR // #endif
  Serial.print("disable_touch_for_x_ms > ");
  Serial.println(x);
#endif
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
      f.write(0xFF);
      // Serial.println(f.readString());
      while (f.available())
      {
        Serial.write(f.read());
      }
      f.close();
    }
  }
  Serial.print(str);
  #ifdef DEBUG_AMOR
  Serial.print("Free Flash>");
  Serial.println(ESP.getFreeSketchSpace());
  #endif
 
}

void setup()
{
#ifdef DEBUG_AMOR
  Serial.begin(115200);
  Serial.println("==DEBUGGING ENABLED==");
  printHeap();
#endif

  setup_ISRs();

  // disable_touch_for_x_ms(1200000);

  setup_RGB_leds();
  // calibrate_setup_touch_sensor();
  wifiManagerSetup();
  digitalWrite(wifiManagerLED, HIGH);

#ifdef DEBUG_AMOR
  printHeap();
#endif

  setup_async();

  listAndReadFiles();

#ifdef DEBUG_AMOR
  printHeap();
#endif
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
}
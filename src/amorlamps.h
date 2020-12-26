#include <Arduino.h>
#include <WiFiManager.h>
#include <FastLED.h>
#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"

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

void setup_async();

void setupUNIXTime();
void amorWebsocket_setup();
void setup_mDNS();

void listAndReadFiles();


// Other methods
void myIRS1_method();
void myIRS2_method();

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void onRequest(AsyncWebServerRequest *request);
void handleFileUpload();


// In void loop
void myIRS_check();
void loop_async();

#endif
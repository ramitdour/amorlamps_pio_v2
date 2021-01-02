/* ================= Import libraries START AVISH================= */

#include <littlefs.h>         // Although littlefs is inported so FS is not required but , in my case I had to add FS
#include <Ticker.h>           //for LED status if Wifi
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
unsigned long reconnect_aws_millis = 0;
// const char *aws_topic = "$aws/things/esp8266_C7ED21/";

// aws_topic;

String aws_topic_str = "$aws/things/" + deviceId + "/";
// char *aws_topic = "$aws/things/esp8266_C7ED21/";
String aws_group_topic_str = "amorgroup/" + groupId + "/";

void aws_callback(char *topic, byte *payload, unsigned int length);

WiFiClientSecure espClient;

PubSubClient clientPubSub(AWS_endpoint, 8883, aws_callback, espClient); //set MQTT port number to 8883 as per //standard

// WebSocketsServer
ESP8266WebServer server;
WebSocketsServer webSocket = WebSocketsServer(81);

char webpage[] PROGMEM = R"=====( hello world webpage!!! )=====";

// ESP8266HTTPUpdateServer httpUpdater;

// BearSSL::CertStore certStore;

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

void publish_boot_data()
{
#ifdef DEBUG_AMOR
  Serial.println("publis_boot_data()");
#endif
}

void subscribeDeviceTopics()
{
#ifdef DEBUG_AMOR
  Serial.println("subscribeDeviceTopics()");
#endif
}

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
  //   if (!fileSystem->begin())
  //   {
  // #ifdef DEBUG_AMOR
  //     printHeap();
  //     Serial.println("Failed to mount file system");
  // #endif
  //     return;
  //   }

  // Load certificate file
  File cert = fileSystem->open("/cert.der", "r"); //replace cert.crt with your uploaded file name
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
  File private_key = fileSystem->open("/private.der", "r"); //replace private with your uploaded file name
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
  File ca = fileSystem->open("/ca.der", "r"); //replace ca with your uploaded file name
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

  // fileSystem->end();

#ifdef DEBUG_AMOR
  printHeap();
#endif
}

bool updateto_givenfile_ConfigJSON(String &key, String &value, String &filename)
{

  File configFile;

  configFile = fileSystem->open(filename, "r");
  if (!configFile)
  {
#ifdef DEBUG_AMOR
    Serial.println("Failed to open config file");
#endif
    return false;
  }

  size_t size = configFile.size();
#ifdef DEBUG_AMOR
  Serial.print("Config file size read only=");
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
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonDocument<1024> doc;

  DeserializationError error = deserializeJson(doc, buf.get());

#ifdef DEBUG_AMOR //
  if (error)
  {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
  }
  // serializeJsonPretty(doc, Serial); // TODO : delete it only to getconfig file data
#endif
  configFile.close();

  // configFile is closed after reading data into doc , now updating the data;

  configFile = fileSystem->open(filename, "w+"); // TODO: w or w+ ?
  if (!configFile)
  {
#ifdef DEBUG_AMOR
    Serial.println("Failed to open config file");
#endif
    return false;
  }

  size = configFile.size();
#ifdef DEBUG_AMOR
  Serial.print("Config file size write =");
  Serial.println(size);
#endif

  if (size > 1024 * 3)
  {
#ifdef DEBUG_AMOR
    Serial.println("Config file size is too large");
#endif
    return false;
  }

  doc[key.c_str()] = value;

// TODO: check wheather it writes to to serial usb port or serially to flash chip
#ifdef DEBUG_AMOR
  Serial.println("serializeJson(doc, configFile);");
#endif

  serializeJson(doc, configFile);

  configFile.close();

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
      return updateto_givenfile_ConfigJSON(key, value, tempStr);
      //break;
    }
  }

  tempStr = fileName; // last value will be "/config9.json";
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println("END updatetoConfigJSON" + key + ":" + value + tempStr);
#endif
  return updateto_givenfile_ConfigJSON(key, value, tempStr);

  // in future will write to any one of 789 only , on basis of size;

  // key not found in any of the file
  // "ERR-KEY";
}

String readFrom_given_ConfigJSON(String &key, String &filename)
{
#ifdef DEBUG_AMOR
  printHeap();
  Serial.println("readFrom_given_ConfigJSON" + key + " from " + filename);
#endif
  // TODO: check is little fs is begun before this call ?

  File configFile = fileSystem->open(filename, "r");
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
  printHeap();
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
  printHeap();

  StaticJsonDocument<512> doc;

  StaticJsonDocument<256> filter;

  printHeap();
  filter[key] = true;
  auto error = deserializeJson(doc, buf.get(), DeserializationOption::Filter(filter));

  printHeap();

  // auto error = deserializeJson(doc, buf.get());

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
  if (doc.containsKey(key))
  {
    const char *value = doc[key];
#ifdef DEBUG_AMOR
    Serial.print("Loaded ");
    Serial.print(key);
    Serial.print(":");
    Serial.println(value);
#endif
    // return true;
    printHeap();
    return value;
  }
  else
  {
    // is file mai key he nhi hai
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

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
#ifdef DEBUG_AMOR
  Serial.println("webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)");
  printHeap();
#endif
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
  Serial.println(msg);
  server.send(500, FPSTR("text/plain"), msg + "\r\n");
}

/*
   Handle a file upload request
*/

File uploadFile;

void handleFileUpload()
{
  Serial.println("handleFileUpload() START");

  String path = server.uri();
  Serial.println(String("handleFileRead: ") + path);

  if (!fsOK)
  {
    Serial.println("error in opening FS");
    return replyServerError(FPSTR("FS_INIT_ERROR"));
  }

  if (server.uri() != "/fsupload")
  {
    Serial.println("path not ends with /fsupload");
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

    Serial.println(String("handleFileUpload Name: ") + filename);

    uploadFile = fileSystem->open(filename, "w+");

    if (!uploadFile)
    {
      Serial.println(String(" CREATE FAILED , filename: ") + filename);
      return replyServerError(F("CREATE FAILED"));
    }
    Serial.println(String("Upload: START, filename: ") + filename);
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    Serial.println("upload.status == UPLOAD_FILE_WRITE");
    if (uploadFile)
    {
      size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);

      Serial.print("bytesWritten>>");
      Serial.println(bytesWritten);

      if (bytesWritten != upload.currentSize)
      {
        Serial.println("WRITE FAILED !!!");
        return replyServerError(F("WRITE FAILED"));
      }
    }
    Serial.println(String("Upload: WRITE, Bytes: ") + upload.currentSize);
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (uploadFile)
    {
      uploadFile.close();
      Serial.println("uploadFile.close(); PROPERLY");
    }
    Serial.println(String("Upload: END/COMPLETED, Size: ") + upload.totalSize);
  }
  Serial.println("handleFileUpload() END");
}

/*
   Read the given file from the filesystem and stream it back to the client
*/
// void handleFileRead(String path)
void handleFileRead()
{
  Serial.println("handleFileRead() START");

  String path = server.uri();
  Serial.println(path);
  Serial.println(String("handleFileRead: ") + path);

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
      Serial.println("Sent less data than expected!");
    }
    file.close();
    Serial.println(" file.close();");

    // return true;
    return;
  }
  // return false;
  replyOKWithMsg("Something is wrong");

  Serial.println("handleFileRead() END");
}

void handleNotFound()
{
  Serial.println("handleNotFound()");

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
  Serial.println(message);
  server.send(404, "text/plain", message);
}

void websocket_server_mdns_setup()
{
#ifdef DEBUG_AMOR
  Serial.println("websocket_server_mdns_setup()");
  printHeap();
#endif
  if (WiFi.status() == WL_CONNECTED)
  {
    // server_setup();
    server.on("/", []() {
      server.send_P(200, "text/html", webpage);
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

    server.serveStatic("/index", LittleFS, "/index.html");
    server.serveStatic("/style", LittleFS, "/style.css");
    server.serveStatic("/script", LittleFS, "/script.js");
    server.serveStatic("/config", LittleFS, "/config.json");

    server.onNotFound(handleNotFound);
    server.begin();

    webSocket.begin();

    webSocket.onEvent(webSocketEvent);
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println("websocket_server_mdns_setup  !WL_CONNECTED");
    printHeap();
#endif
    restart_device();
  }
#ifdef DEBUG_AMOR
  Serial.println("websocket_server_mdns_setup() done");
  printHeap();
#endif
}

// DigiCert High Assurance EV Root CA
const char trustRoot[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBs
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j
ZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDTMxMTExMDAwMDAwMFowbDEL
MAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3
LmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2Ug
RVYgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm
+9S75S0tMqbf5YE/yc0lSbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTW
PNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtqumX8OkhPhPYlG++MXs2ziS4wblCJEM
xChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0AFFoEt7oT61EKmEFB
Ik5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx+mM0aBhakaHPQNAQTXKFx01p8VdteZOE3
hzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsg
EsxBu24LUTi4S8sCAwEAAaNjMGEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQF
MAMBAf8wHQYDVR0OBBYEFLE+w2kD+L9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaA
FLE+w2kD+L9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3DQEBBQUAA4IBAQAcGgaX3Nec
nzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe/EW1ntlMMUu4kehDLI6z
eM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jF
hS9OMPagMRYjyOfiZRYzy78aG6A9+MpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2
Yzi9RKR/5CYrCsSXaQ3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2/S6cCZdkGCe
vEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep
+OkuE6N36B9K
-----END CERTIFICATE-----
)EOF";

void download_file_to_fs()
{
#ifdef DEBUG_AMOR
  Serial.print("firmware_update_from_fs()");
  printHeap();
#endif

  espClient.stopAll();
  printHeap();

  webSocket.~WebSocketsServer();
  clientPubSub.~PubSubClient();
  //
  server.~ESP8266WebServerTemplate();
  webSocket.~WebSocketsServer();

  Serial.println("~PubSubClient");

  printHeap();

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
  Serial.print("host_ca >");
  Serial.println(host_ca);
  Serial.print("host >");
  Serial.println(host);
  Serial.print("httpsPort >");
  Serial.println(httpsPort);
  Serial.print("d2fs_url_file >");
  Serial.println(d2fs_url_file);
  Serial.print("d2fs_finished_flag >");
  Serial.println(d2fs_finished_flag);
  Serial.print("filename >");
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
    Serial.println("Something is null");
    return;
  }

  // Load private key file
  if (!host_ca.startsWith("/"))
  {
    host_ca = "/" + host_ca;
  }

#ifdef DEBUG_AMOR
  Serial.print("host_ca > = ");
  Serial.println(host_ca);

  printHeap();
#endif

  File ota_ca_file = fileSystem->open(host_ca, "r"); //replace private with your uploaded file name
  if (!ota_ca_file)
  {
#ifdef DEBUG_AMOR
    Serial.println("Failed to open host_ca cert file");
    printHeap();
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println("Success to open host_ca cert file");
    printHeap();
#endif
  }

  delay(1000);

  if (espClient.loadCACert(ota_ca_file))
  {
#ifdef DEBUG_AMOR
    Serial.println("host_ca cert loaded");
    printHeap();
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println("host_ca cert not loaded");
    printHeap();
#endif
  }

  ota_ca_file.close();

#ifdef DEBUG_AMOR
  Serial.println("ota_ca_file.close();");
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
    Serial.println("Connection to host failed");
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
  //       Serial.println("Headers received");
  //       printHeap();
  // #endif
  //       break;
  //     }

  //     char buf[128];
  //     espClient.readBytes(buf, 120);

  //     String x = buf;
  //     Serial.println(x);

  //     // Serial.println("+++++++Started++++++++");
  //     // Serial.print(String(buf));
  //     // Serial.println("+++++++END++++++++");
  //   }

  //   espClient.status();

  //   // String payload = espClient.readStringUntil('\n');

  int contentLength = -1;
  int httpCode;
  espClient.setInsecure(); // TODO: what this does ?

  while (espClient.connected())
  {
    String header = espClient.readStringUntil('\n');
    Serial.print("h...");
    Serial.println(header);
    if (header.startsWith(F("HTTP/1.")))
    {
      httpCode = header.substring(9, 12).toInt();
      Serial.print("httpCode ..>>");
      Serial.println(httpCode);

      if (httpCode != 200)
      {
        Serial.println(String(F("HTTP GET code=")) + String(httpCode));
        espClient.stop();
        // return -1;
        return;
      }
    }
    if (header.startsWith(F("Content-Length: ")))
    {
      contentLength = header.substring(15).toInt();
      Serial.print("contentLength ..>>");
      Serial.println(contentLength);
    }
    if (header == F("\r"))
    {
      break;
    }
  }

  if (!(contentLength > 0))
  {
    Serial.println(F("HTTP content length=0"));
    espClient.stop();
    // return -1;
    return;
  }

  // Open file for write
  updatetoConfigJSON("d2fs_finished_flag", "0");

  fs::File f = fileSystem->open(filename, "w+");
  if (!f)
  {
    Serial.println(F("file open failed"));
    espClient.stop();
    // return -1;
    return;
  }

  // Download file
  int remaining = contentLength;
  int received;
  uint8_t buff[128] = {0};

  // read all data from server
  while (espClient.status() == ESTABLISHED && remaining > 0)
  {
    // read up to buffer size
    received = espClient.readBytes(buff, ((remaining > sizeof(buff)) ? sizeof(buff) : remaining));

    // write it to file
    f.write(buff, received);

    if (remaining > 0)
    {
      remaining -= received;
    }
    yield();

    // Serial.write(buff, received);
    // if (remaining > 0)
    // {
    //   remaining -= received;
    // }

    Serial.println("espClient status --- " + String(espClient.status()));
    printHeap();
  }

  if (remaining != 0)
    Serial.println(" Not recieved full data remaing =" + String(remaining) + "/" + String(contentLength));

  // Close SPIFFS file
  f.close();

  // Stop client
  espClient.stop();

  // return (remaining == 0 ? contentLength : -1);
  if (remaining == 0)
  {
    Serial.println(contentLength);
    updatetoConfigJSON("d2fs_finished_flag", "1");
  }
  else
  {
    updatetoConfigJSON("d2fs_finished_flag", "0");
    Serial.println("--1");
  }
  // return;

#ifdef DEBUG_AMOR
  Serial.println("void firmware_update_from_fs(String &ota_filename); END");
  printHeap();
#endif

  restart_device();
}

void firmware_update_from_config()
{

#ifdef DEBUG_AMOR
  Serial.println("firmware_update_from_config START");
  printHeap();
#endif

  // espClient.~WiFiClientSecure();
  espClient.stopAll();
  printHeap();

  webSocket.~WebSocketsServer();
  clientPubSub.~PubSubClient();
  //
  server.~ESP8266WebServerTemplate();
  webSocket.~WebSocketsServer();

  Serial.println("~PubSubClient");

  printHeap();

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
    Serial.println("Something is null");
  }

#ifdef DEBUG_AMOR
  Serial.print("host_ca >");
  Serial.println(host_ca);
  Serial.print("host >");
  Serial.println(host);
  Serial.print("httpsPort >");
  Serial.println(httpsPort);
  Serial.print("URL_fw_Version >");
  Serial.println(URL_fw_Version);
  Serial.print("URL_fw_Bin >");
  Serial.println(URL_fw_Bin);
  printHeap();
#endif

  // Load private key file
  if (!host_ca.startsWith("/"))
  {
    host_ca = "/" + host_ca;
  }

#ifdef DEBUG_AMOR
  Serial.print("host_ca > = ");
  Serial.println(host_ca);

  printHeap();
#endif

  File ota_ca_file = fileSystem->open(host_ca, "r"); //replace private with your uploaded file name
  if (!ota_ca_file)
  {
#ifdef DEBUG_AMOR
    Serial.println("Failed to open host_ca cert file");
    printHeap();
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println("Success to open host_ca cert file");
    printHeap();
#endif
  }

  delay(1000);

  if (espClient.loadCACert(ota_ca_file))
  {
#ifdef DEBUG_AMOR
    Serial.println("host_ca cert loaded");
    printHeap();
#endif
  }
  else
  {
#ifdef DEBUG_AMOR
    Serial.println("host_ca cert not loaded");
    printHeap();
#endif
  }

  ota_ca_file.close();

#ifdef DEBUG_AMOR
  Serial.println("ota_ca_file.close();");
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
    Serial.println("Connection to host failed");
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
      Serial.println("Headers received");
      printHeap();
#endif
      break;
    }
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
  Serial.print("currentFW=");
  Serial.println(currentFW);
  Serial.print("gotFW=");
  Serial.println(gotFW);
  printHeap();
#endif

  if (gotFW > currentFW)
  {
// UPDATE FW
#ifdef DEBUG_AMOR
    printHeap();
#endif
    Serial.println("New firmware detected");

    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

#ifdef DEBUG_AMOR
    printHeap();
#endif

    t_httpUpdate_return ret = ESPhttpUpdate.update(espClient, URL_fw_Bin);

#ifdef DEBUG_AMOR
    printHeap();
#endif

    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
    }
  }
  else
  {
    if (currentFW == gotFW)
    {
      Serial.println("Device already on latest firmware version");
    }
    else
    {
      Serial.println("Device already on latest firmware version got FW is old");
    }
  }

  // if(payload.equals(FirmwareVer))
  // {
  //   Serial.println("Device already on latest firmware version");
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
  Serial.println("setupUNIXTime");
#endif
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
    // restart_device(); TODO:Why not this ?
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
      Serial.println("===got my file");
      File f = dir.openFile("r");
      // Serial.println(f.readString());
      while (f.available())
      {
        Serial.write(f.read());
      }
      f.close();

      // for LOGGING
      // File f2 = dir.openFile("a");
      // // Serial.println(f.readString());
      // if (!f2)
      // {
      //   Serial.println("Failed to open /stats.txt");
      //   return;
      // }
      // f2.println(timeClient.getEpochTime());
      // f2.println(timeClient.getFormattedTime());
      // f2.println(ESP.getResetInfo());
      // f2.println("");
      // f2.close();
      // Serial.println("LOGS UPDATED");
    }
  }
  Serial.print(str);
  // fileSystem->end();
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
  Serial.println("fileSystem->begin(); START");
#endif

  fileSystemConfig.setAutoFormat(false);
  fileSystem->setConfig(fileSystemConfig);
  fsOK = fileSystem->begin();

  if (fsOK)
  {
#ifdef DEBUG_AMOR

    printHeap();
    Serial.println("Failed to mount file system");
#endif
    fileSystem->begin();
  }

#ifdef DEBUG_AMOR
  Serial.println(fsOK ? F("Filesystem initialized.") : F("Filesystem init failed!"));

  printHeap();
  Serial.println("setup_config_vars START");
#endif

  setup_config_vars();

#ifdef DEBUG_AMOR
  Serial.println("setup_config_vars END");
  printHeap();
  Serial.println("listAndReadFiles START");
#endif

  listAndReadFiles(); // TODO: conmment related code in production.

#ifdef DEBUG_AMOR
  Serial.println("listAndReadFiles END");
  printHeap();
  Serial.println("readAwsCerts START");
#endif

  readAwsCerts();

  // fileSystem->end();

#ifdef DEBUG_AMOR
  Serial.println("readAwsCerts END ,  fileSystem->end();");
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
  Serial.println("void Setup end");
#endif

#ifdef DEBUG_AMOR
  Serial.println("setup_ISRs,setup_RGB_leds, wifiManagerSetup END");
  printHeap();
  Serial.println("setupUNIXTime START");
#endif

  setupUNIXTime();

#ifdef DEBUG_AMOR
  Serial.println("setupUNIXTime END");
  printHeap();
  Serial.println("amorWebsocket_setup START");
#endif

  websocket_server_mdns_setup();

#ifdef DEBUG_AMOR
  Serial.println("amorWebsocket_setup END");
  printHeap();
  Serial.println("setup_mDNS START");
#endif

  // setup_mDNS(); already done above

#ifdef DEBUG_AMOR
  Serial.println("setup_mDNS END");
  printHeap();
  Serial.println("setup_mDNS START");
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

  readFromConfigJSON("apSSID");
  updatetoConfigJSON("apSSID", "update1");
  readFromConfigJSON("apSSID");
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
  Serial.println(" !!! FORGOT Wifi Id pass :(  !!!");
  Serial.println(ESP.getFreeHeap());
#endif

  delay(500);
  restart_device();
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
    if (millis() - reconnect_aws_millis > 5000)
    {
      // clientPubSub_connected_counter = 0;

      reconnect_aws_millis = millis();

      leds[NUM_LEDS - 1] = CRGB::MediumVioletRed;
      FastLED.show();

#ifdef DEBUG_AMOR
      // Serial.print(clientPubSub_connected_counter);
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
        // Serial.println("espClient.flush();");
        // espClient.flush();
        // printHeap();
        // Serial.println("espClient.disableKeepAlive();");
        // espClient.disableKeepAlive();
        // printHeap();
        // Serial.println("espClient.stop();");
        // espClient.stop();
        // printHeap();
        // Serial.println("espClient.stopAll();");
        // espClient.stopAll();
        // printHeap();

        // TODO : delete ,not for production
        // Once connected, publish an announcement...
        clientPubSub.publish("outTopic", "hello world");
        // ... and resubscribe
        clientPubSub.subscribe("inTopic");

        Serial.println("subscribeDeviceShadow START");
#endif

        // subscribeDeviceShadow();

#ifdef DEBUG_AMOR
        Serial.println("subscribeDeviceShadow END");
        printHeap();
        Serial.println("setup_config_vars START");
#endif

        setup_config_vars();

#ifdef DEBUG_AMOR
        Serial.println("setup_config_vars END");
        printHeap();
        Serial.println("publish_boot_data START");
#endif

        publish_boot_data();

#ifdef DEBUG_AMOR
        Serial.println("publish_boot_data END");
        printHeap();
        Serial.println("subscribeDeviceTopics START");
#endif

        subscribeDeviceTopics();

#ifdef DEBUG_AMOR
        Serial.println("subscribeDeviceShadow END");
        printHeap();
        Serial.println("subscribeDeviceShadow START");
#endif

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
        if (failed_aws_trials_counter > failed_aws_trials_counter_base)
        {
          //TODO:increase the fail counter count so that user can update its certificates
          // do i need to disable it so that user can update its its end point and certificates
          restart_device();
        }

#ifdef DEBUG_AMOR
        printHeap();
        Serial.print("failed, rc=");
        Serial.print(clientPubSub.state());
        Serial.println(" try again in 5 seconds");
        char buf[256];
        espClient.getLastSSLError(buf, 256);
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

void loop()
{
  // check flags is there any interrupt calls made

  myIRS_check();

  setupUNIXTimeLoop();

  // check_AWS_mqtt(); //TODO:uncomment

  // server and dns loops
  // websocket_server_mdns_loop(); //TODO:uncomment
}
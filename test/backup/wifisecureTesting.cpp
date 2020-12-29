#include <littlefs.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h> // For HTTP client functionality of ESP8266
#include <NTPClient.h>         // To connect Network Time Protocol (NTP) server for epoch time
#include <WiFiUdp.h>

WiFiClientSecure espClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// DigiCert High Assurance EV Root CA
const char trustRoot[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEArTxNPkTKZEc2wwTquDULwmPP8RflL+fRG79yo1Km12oxPlQJ
2UY69AUbbpIuBvWqW/EjIjAShucSdtiX6s9tNOsfpez6ksZUE7tREzsjKjHs7KlB
nnd7StpizOUgAz1uRN/ics6NwqWP8ICxo0kyA5xgK553ygK8BLckYGdbHhTi802U
0NiLojB7HSzY2seL+vcHXhP2amtq8Mcdi9L7K644wrLQYWL2LGquN+n2uszsGwq7
HZvRK2CDFIirNhBASk2MKdaO4f6XM6mikB34MOJcXW06PLYNTxdsExxJAyfWlpK8
qrnC4cgqLkckUloTy/3emAj1Bj1bl/B6ClMb1wIDAQABAoIBAD+KTLK4PJwHWtix
Jv6lxkhon87IevHVT2ekEwJAwJ1mf+ViobImBl2WufcWevdmiDPtAHv7se/1NVqO
GzqvFllAnH0ZrNbFE/Wg0R4CpOVHJqWvdzgOjxRngbGxoMpPb/CnRcHibTCl1WPb
cEx5/nRAmS2mlD1uP1RjcF5DyrH/UKrrvqgMuOBG4ty+SSeEWuxErUjHy9bGGlwc
JKF1gG3vaOe4ecoel2rU78Huubbr62pKCHIFIhTZ9qkvOcs3feinYDIzLMFwKECW
1alipcQDDHLRIaa0ekawsxuJ5rcaFxBgTgzqnv61J2NOT//MvYdXp/XECbW3WPeN
cvUd1FECgYEA2KK0MSpi/BKc2FTdcLJE/eyZWF3Jrm303FCzmcHi+1Tfu9oHvM2B
yfmzUzJ/bmHeJqZhkwK2VaEBjIOxhKU+yUYaw+P1TGa/TJdBldOLT6yhRfvuk3eX
IsEryoGJvsDcG3z4t7ZohiIQEbBCySHo4oMm17l1Ql1HPDvFlArJqZ0CgYEAzLa/
ad81P1X8Lvr9s+FlU8Cga4edylVupJU6uDCqApJJlzkfnKJhbCq/FL6W/Rgkp95K
mZFIvNhkFIscrvazrRLA7UZgYwrBGDVIkPZViZUi276UGFzQsZDDofMSP/SLLj2g
QW6ntvOwRxdFWUVkogXBPqjGLv0Kf7suQiJf6wMCgYEA08lMq/wqRRDVMVDWI8TE
WhIiYBdgghyRE4n560l3ZAo3qGigw92NEy4AOEfX+MvI6LQkFBrEsrXy07Izq9/4
n8Dfjb6gIw8X2gLNZXIocb3s9IQ1WwnBQYLkEtfNGCVniaAFb2TujqNiXkiZIhT6
nedl4+Q1VKnoaGRu5ioduKkCgYEAh4zeU/kzZdvGn0kconY1xO5AitMCvU4ydBJI
DlxSxl8dEWAGuY2f2qK3YcINcksBQpJjbSoRMtJJ+nxeos/CC3DmhHZcLliZoTDu
+uXRn8c7jKu1nigfG+RSVbMAu45udlDiA1GFBhR3/arABat4Rfxvh9DzYXu17vv/
VlUylAcCgYAxZEcstKDTHSWmalCfL7DtinIOzRAOaepbE6yFX2TbdCYZtPXubfFu
+2ozGmqBTqxJqN2+fmBJtg0bhTWp99Yzw5l1w6yIw40yNkUXnrYN0pglNsNoRmf5
1Q7SRzNxxH6TdY0gB8ctKlGZv3wZyEYmSzon3eatyM0jjZ+dRHRpXA==
-----END RSA PRIVATE KEY-----
)EOF";

const char rsaP[] PROGMEM = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEArTxNPkTKZEc2wwTquDULwmPP8RflL+fRG79yo1Km12oxPlQJ
2UY69AUbbpIuBvWqW/EjIjAShucSdtiX6s9tNOsfpez6ksZUE7tREzsjKjHs7KlB
nnd7StpizOUgAz1uRN/ics6NwqWP8ICxo0kyA5xgK553ygK8BLckYGdbHhTi802U
0NiLojB7HSzY2seL+vcHXhP2amtq8Mcdi9L7K644wrLQYWL2LGquN+n2uszsGwq7
HZvRK2CDFIirNhBASk2MKdaO4f6XM6mikB34MOJcXW06PLYNTxdsExxJAyfWlpK8
qrnC4cgqLkckUloTy/3emAj1Bj1bl/B6ClMb1wIDAQABAoIBAD+KTLK4PJwHWtix
Jv6lxkhon87IevHVT2ekEwJAwJ1mf+ViobImBl2WufcWevdmiDPtAHv7se/1NVqO
GzqvFllAnH0ZrNbFE/Wg0R4CpOVHJqWvdzgOjxRngbGxoMpPb/CnRcHibTCl1WPb
cEx5/nRAmS2mlD1uP1RjcF5DyrH/UKrrvqgMuOBG4ty+SSeEWuxErUjHy9bGGlwc
JKF1gG3vaOe4ecoel2rU78Huubbr62pKCHIFIhTZ9qkvOcs3feinYDIzLMFwKECW
1alipcQDDHLRIaa0ekawsxuJ5rcaFxBgTgzqnv61J2NOT//MvYdXp/XECbW3WPeN
cvUd1FECgYEA2KK0MSpi/BKc2FTdcLJE/eyZWF3Jrm303FCzmcHi+1Tfu9oHvM2B
yfmzUzJ/bmHeJqZhkwK2VaEBjIOxhKU+yUYaw+P1TGa/TJdBldOLT6yhRfvuk3eX
IsEryoGJvsDcG3z4t7ZohiIQEbBCySHo4oMm17l1Ql1HPDvFlArJqZ0CgYEAzLa/
ad81P1X8Lvr9s+FlU8Cga4edylVupJU6uDCqApJJlzkfnKJhbCq/FL6W/Rgkp95K
mZFIvNhkFIscrvazrRLA7UZgYwrBGDVIkPZViZUi276UGFzQsZDDofMSP/SLLj2g
QW6ntvOwRxdFWUVkogXBPqjGLv0Kf7suQiJf6wMCgYEA08lMq/wqRRDVMVDWI8TE
WhIiYBdgghyRE4n560l3ZAo3qGigw92NEy4AOEfX+MvI6LQkFBrEsrXy07Izq9/4
n8Dfjb6gIw8X2gLNZXIocb3s9IQ1WwnBQYLkEtfNGCVniaAFb2TujqNiXkiZIhT6
nedl4+Q1VKnoaGRu5ioduKkCgYEAh4zeU/kzZdvGn0kconY1xO5AitMCvU4ydBJI
DlxSxl8dEWAGuY2f2qK3YcINcksBQpJjbSoRMtJJ+nxeos/CC3DmhHZcLliZoTDu
+uXRn8c7jKu1nigfG+RSVbMAu45udlDiA1GFBhR3/arABat4Rfxvh9DzYXu17vv/
VlUylAcCgYAxZEcstKDTHSWmalCfL7DtinIOzRAOaepbE6yFX2TbdCYZtPXubfFu
+2ozGmqBTqxJqN2+fmBJtg0bhTWp99Yzw5l1w6yIw40yNkUXnrYN0pglNsNoRmf5
1Q7SRzNxxH6TdY0gB8ctKlGZv3wZyEYmSzon3eatyM0jjZ+dRHRpXA==
-----END RSA PRIVATE KEY-----
)EOF";

const char rsaC[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDWTCCAkGgAwIBAgIUX0LQOaZL/tw8eLuPGFItEQ4LKAIwDQYJKoZIhvcNAQEL
BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g
SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTIwMTIyMDA4NDE0
OFoXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0
ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAK08TT5EymRHNsME6rg1
C8Jjz/EX5S/n0Ru/cqNSptdqMT5UCdlGOvQFG26SLgb1qlvxIyIwEobnEnbYl+rP
bTTrH6Xs+pLGVBO7URM7Iyox7OypQZ53e0raYszlIAM9bkTf4nLOjcKlj/CAsaNJ
MgOcYCued8oCvAS3JGBnWx4U4vNNlNDYi6Iwex0s2NrHi/r3B14T9mpravDHHYvS
+yuuOMKy0GFi9ixqrjfp9rrM7BsKux2b0StggxSIqzYQQEpNjCnWjuH+lzOpopAd
+DDiXF1tOjy2DU8XbBMcSQMn1paSvKq5wuHIKi5HJFJaE8v93pgI9QY9W5fwegpT
G9cCAwEAAaNgMF4wHwYDVR0jBBgwFoAU2yGdqgF4ghIhuBETtFG0ENEbx6EwHQYD
VR0OBBYEFLlvs3xL5Gew6LGq9ewZT2pqnFLAMAwGA1UdEwEB/wQCMAAwDgYDVR0P
AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQCf6YPmzO64c0KJEYbILO6WEJOJ
fhwtuZFByDkn2Hgrt9Rc19pVNCJLtsZ8KIWO6tYAHxYF/v2sTkqSbEnw1QaNhVvK
O8YVs96zYPi2n2zAAGHFR/Lfk0rEdQba65hUhRYAQW4e9CuCDi1Q/wf54vOMkpLx
BuGfwzDi/l3/hrhjJAdB+w4SjiUqdHFu1mi2FEMQmwCAelqmv9WG0TYk7T7aBoOw
0Msk5IMt90gThZWVSbi3ZQZLtK3Jb/upJU6JTSmnAlmNyP+POvA3JiLdH7WgPVLO
OvDL8oGunnw+EuXmk2M5E/210TWmmLwZUk/v8fd0M9qSzhbmsJPPrTmpZnZk
-----END CERTIFICATE-----
)EOF";

const char *host = "a3an4l5rg1sm5p-ats.iot.ap-south-1.amazonaws.com";
const int httpsPort = 8883;
void connectotserver()
{

  if (!espClient.connected())
  {
    if (!espClient.connect(host, httpsPort))
    {
      Serial.println("Connection failed");
      Serial.println(ESP.getFreeHeap());
      return;
    }
    else
    {
      Serial.println("Connection Sucessfull");
      Serial.println(ESP.getFreeHeap());
      return;
    }
  }
}

X509List cert(trustRoot);

void setup()
{
  Serial.begin(115200);
  Serial.println(ESP.getFreeHeap());
  LittleFS.begin();
  Serial.println(ESP.getFreeHeap());

  timeClient.begin();

  while (!timeClient.update())
  {
    timeClient.forceUpdate();
  }

  espClient.setX509Time(timeClient.getEpochTime());
  Serial.println(ESP.getFreeHeap());
  Serial.println(timeClient.getEpochTime());
  Serial.println(ESP.getFreeHeap());

  Serial.println(ESP.getFreeHeap());
  espClient.setTrustAnchors(&cert);
  Serial.println(ESP.getFreeHeap());

  // X509List certP(rsaP);
  // X509List certC(rsaC);
  // PrivateKey certP(rsaP);

  espClient.setClientRSACert(new X509List(rsaC), new PrivateKey(rsaP));
  Serial.println(ESP.getFreeHeap());

  // File f = LittleFS.open("/private.der", "r");
  // Serial.println(ESP.getFreeHeap());
  // espClient.loadPrivateKey(f);
  // f.close();
  // Serial.println(ESP.getFreeHeap());

  // f = LittleFS.open("/cert.der", "r");
  // Serial.println(ESP.getFreeHeap());
  // espClient.loadCertificate(f);
  // f.close();
  // Serial.println(ESP.getFreeHeap());

  // f = LittleFS.open("/ca.der", "r");
  // Serial.println(ESP.getFreeHeap());
  // espClient.loadCACert(f);
  // f.close();
  // Serial.println(ESP.getFreeHeap());

  LittleFS.end();
  Serial.println(ESP.getFreeHeap());
}

void loop()
{
  // Serial.println(ESP.getFreeHeap());
  connectotserver();

  // delay(5000);
}

//Using .der files
// 44832
// 44616
// 43816
// 1609102339
// 43816
// 43512
// 43048
// 42752
// 41768
// 41472
// 40488
// 40704
// 40704
// Connection Sucessfull
// 19616
// 19112
// Connection Sucessfull
// 19000
// 19112
// Connection Sucessfull
// 19112
// 19112
// Connection Sucessfull
// 19112
// 19112
// Connection Sucessfull
// 19112
// 19112
// Connection Sucessfull
// 19112
// 19112
// Connection Sucessfull
// 19112
// 19112
// Connection Sucessfull

// 44832
// 44616
// 43816
// 1609102828
// 43816
// 43512
// 43048
// 42752
// 41768
// 41984
// 41984
// Connection Sucessfull
// 19656
// 20392
// Connection Sucessfull
// 19152
// 20392
// Connection Sucessfull
// 19152
// 20392
// Connection Sucessfull
// 19152
// 20392
// Connection Sucessfull
// 19152
// 20392

//USING cert files
// 44864
// 44648
// 43848
// 1609103260
// 43848
// 44064
// 44064
// Connection Sucessfull
// 19736
// 20472
// Connection Sucessfull
// 17232

//It can be boserved that SSL connection to aws ca nuse upto 20kB of ram/heap
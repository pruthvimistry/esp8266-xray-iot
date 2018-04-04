#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "FS.h"

#define WIFI_SSID "BHNTG1682GE5F8"
#define WIFI_PASSWORD "d338d64d"
//#define MAC "11:22:33:44:55:66"
#define LOCAL_FILE_NAME "local_store.csv"

//#define FORMAT

#define DEBUG

#define URL "http://192.168.0.6:5000"

String MAC = WiFi.macAddress();

String getPayloadReq(unsigned int readingTime, unsigned int CPM);
void storePayload(unsigned int readingTime, unsigned int CPM);
void ICACHE_RAM_ATTR handleInterrupt();
String getValue(String data, char separator, int index);


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);


const byte interruptPin = D1;
volatile unsigned int interruptCounter = 0;
String payloadReq;
bool stored = false;  // true if there is something in offline storage
File f;

void setup() {

  Serial.begin(115200);//Serial connection

  
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, FALLING);
  
  unsigned int wifi_beg_time = millis();
  Serial.println();
  Serial.println();
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);   //WiFi connection
  WiFi.setAutoReconnect(false);
  Serial.println("Connected in:");
  for(int i=0;i<30;i++){
    if(WiFi.status() != WL_CONNECTED) {  //Wait for the WiFI connection completion
      delay(500);
      Serial.println("Waiting for connection");
    }
  }
  Serial.println(millis() - wifi_beg_time);
  WiFi.setAutoReconnect(false);
//  WiFi.setAutoConnect(true);
  WiFi.printDiag(Serial);
  timeClient.begin();
  timeClient.setUpdateInterval(3600000);
  yield();
  timeClient.forceUpdate();
  Serial.println("Time Started:");
  Serial.println(timeClient.getFormattedTime());

  if (!SPIFFS.begin()) {
    Serial.println("Unable to SPIFFS.begin");
    return;
  }
  #ifdef FORMAT
    SPIFFS.format();
    Serial.println("Formatted SPIFFS");
  #endif
}

void loop() {
  unsigned long init_time = millis();
  String line, payloadRes;
  int httpCode;
  HTTPClient http;    //Declare object of class HTTPClient

  Serial.println("Counting");
  interruptCounter = 0;

  // Take reading for 15 seconds
  while (millis() - init_time <= 15000) {
    delay(2000);
    yield();
    Serial.print('.');
    yield();
  }


  unsigned int currentCPM = (unsigned int)(interruptCounter << 2);
  timeClient.update();
  unsigned int currentReadingTime = timeClient.getEpochTime();


  Serial.println("Count/min is:");
  Serial.println(currentCPM);

  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
    // If local data is stored
    if (stored) {
      Serial.println("SENDING LOCALLY STORED DATA");
      Serial.println(timeClient.getFormattedTime());
      if (!SPIFFS.exists(LOCAL_FILE_NAME)) {
        // if there is an issue
        Serial.println("SPIFFS BEGIN FAILED");
      } else {
        // Open file
        f = SPIFFS.open(LOCAL_FILE_NAME, "r");
        stored = false;   // set stored variable to false so that it doesn't send again
        while (f.available()) {
          line = (f.readStringUntil('\n')); // Read Line
          payloadReq = getPayloadReq(getValue(line, ',', 0).toInt(), getValue(line, ',', 1).toInt());
          Serial.println("LOCAL PAYLOAD");
          Serial.println(payloadReq);
          http.begin(URL);      //Specify request destination
          http.addHeader("Content-Type", "application/json");  //Specify content-type header

          httpCode = http.POST(payloadReq);   //Send the request
          payloadRes = http.getString();                  //Get the response payload
          if (httpCode != 200) {
            stored = true;  // In case there's an error sending, set stored true again
          }
          Serial.println(httpCode);   //Print HTTP return code
          Serial.println(payloadRes);    //Print request response payload

          http.end();  //Close connection
        }
        f.close();
        if (!stored) { // If all local data is sent, delete local file.
          SPIFFS.remove(LOCAL_FILE_NAME);
        }
      }
    }
    // Send current reading.
    Serial.println("Sending Current Data");
    Serial.println(timeClient.getFormattedTime());
    http.begin(URL);      //Specify request destination
    http.addHeader("Content-Type", "application/json");  //Specify content-type header

    payloadReq = getPayloadReq(currentReadingTime, currentCPM);
    int httpCode = http.POST(payloadReq);   //Send the request
    String payloadRes = http.getString();                  //Get the response payload

    Serial.println(httpCode);   //Print HTTP return code
    Serial.println(payloadRes);    //Print request response payload

    http.end();  //Close connection

//    WiFi.disconnect(true);  //FOR TESTING
//    WiFi.mode(WIFI_OFF);
  } else {
    // In case wifi si disconnected
    Serial.println("Error in WiFi connection");
    Serial.println(timeClient.getFormattedTime());
    storePayload(currentReadingTime, currentCPM);
    
    // WiFi reconnect
    delay(1000);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);   //WiFi connection
    delay(1000);
    for(int i=0;i<20;i++){
      if(WiFi.status() != WL_CONNECTED) {  //Wait for the WiFI connection completion
        Serial.println("Waiting for connection");
        delay(3000);
      }else{
        Serial.println("Connected");
        break;
      }
    }
  }
}

// Create payload

String getPayloadReq(unsigned int readingTime, unsigned int CPM) {
  return String("{\"timestamp\":") + String(readingTime) + ",\"MAC\":\"" + MAC + "\", \"CPM\":" + String(CPM) + "}";
}

// Interrupt handler
void ICACHE_RAM_ATTR handleInterrupt() {
  ++interruptCounter;
}


// Stores payload data to internal storage
void storePayload(unsigned int readingTime, unsigned int CPM) {
  yield();
  Serial.println(" OFFLINE");
  f = SPIFFS.open(LOCAL_FILE_NAME, "a");
  f.print(readingTime);
  f.print(',');
  f.print(CPM);
  f.print('\n');
  f.close();
  stored = true;
}


// Gets specified field from charater seperated data.
// example getValue("zero,one,two",',',2) will return "three"
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

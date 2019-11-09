#include "pms.h"
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h> 
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>

Pmsx003 pms(4, 5);

ESP8266WiFiMulti wifiMulti;
ESP8266WebServer server(80);

void handleWake();
void handleRead();
void handleSleep();

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  delay(1000);

  pms.begin();
  pms.waitForData(Pmsx003::wakeupTime);
  pms.write(Pmsx003::cmdModePassive);
  pms.write(Pmsx003::cmdSleep);

  wifiMulti.addAP("", "");

  Serial.println("Connecting ...");
  int i = 0;
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(250);
    Serial.print('.');
  }

  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());              // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer

  if (MDNS.begin("esp-pms")) {
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  server.on("/wake", handleWake);
  server.on("/read", handleRead);
  server.on("/sleep", handleSleep);

  server.begin();
}

void loop() {
  server.handleClient();
}

auto lastRead = millis();

void handleWake() {
  pms.write(Pmsx003::cmdWakeup);
  pms.waitForData(Pmsx003::wakeupTime);
  server.send(404, "application/json", "{\"status\": true, \"message\": \"device woken\"}");
}

void handleSleep() {
  pms.write(Pmsx003::cmdSleep);
  server.send(404, "application/json", "{\"status\": true, \"message\": \"device slept\"}");
}

void handleRead() {
  const auto n = Pmsx003::Reserved;
  Pmsx003::pmsData data[n];

  pms.write(Pmsx003::cmdReadData);

  Pmsx003::PmsStatus status = pms.read(data, n);
  
  switch(status) {
    case Pmsx003::OK:
    {
      Serial.println("_________________");
      auto newRead = millis();
      Serial.print("Wait time ");
      Serial.println(newRead - lastRead);
      lastRead = newRead;

      // For loop starts from 3
      // Skip the first three data (PM1dot0CF1, PM2dot5CF1, PM10CF1)
      for (size_t i = Pmsx003::PM1dot0; i < n; ++i) { 
        Serial.print(data[i]);
        Serial.print("\t");
        Serial.print(Pmsx003::dataNames[i]);
        Serial.print(" [");
        Serial.print(Pmsx003::metrics[i]);
        Serial.print("]");
        Serial.println();
      }

      char sbuf[100];
      sprintf(sbuf, "{\"status\": false, \"message\": \"ok\", \"data\": {\"pm1\": %d, \"pm25\": %d, \"pm10\": %d}}", data[Pmsx003::PM1dot0], data[Pmsx003::PM2dot5], data[Pmsx003::PM10dot0]);
      server.send(200, "application/json", sbuf);

      server.send(200, "application/json", sbuf);
    }
    case Pmsx003::noData:
      server.send(404, "application/json", "{\"status\": false, \"message\": \"no data\"}");
      break;
    default:
      char buf[100];
      sprintf(buf, "{\"status\": false, \"message\": \"%s\"}", Pmsx003::errorMsg[status]);
      server.send(404, "application/json", buf);
      Serial.println("_________________");
      Serial.println(Pmsx003::errorMsg[status]);
      break;
  };
}

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoOTA.h>
#include <MQTT.h>
#include "PMS.h"
#include <SoftwareSerial.h>

#define SEALEVELPRESSURE_HPA (1013.25)
#define HOSTNAME "esp-corner-end"

#define PMS_READ_STABLE_DELAY 30000
#define PMS_UPDATE_INTERVAL 120000

#define PIN_RX  D5
#define PIN_TX  D6

Adafruit_BME280 bme;

WiFiClient wifiClient;
MQTTClient mqttClient;

SoftwareSerial pmsSerial(PIN_RX, PIN_TX);
PMS pms(pmsSerial);
PMS::DATA data;

int rainPin = A0;
int thresholdValue = 400;

const char* kSsid = "";
const char* kPassword = "";

void setup() {
  Serial.begin(115200);

  // setup Rain sensor
  pinMode(rainPin, INPUT);

  // Setup BME280
  bool status = bme.begin(0x76);
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  // Setup PMS5003
  pmsSerial.begin(9600);
  pms.passiveMode();

  WiFi.mode(WIFI_STA);
  WiFi.begin(kSsid, kPassword);
  WiFi.hostname(HOSTNAME);

  Serial.print("Connecting to WiFi..");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("Connected to ");
  Serial.println(kSsid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP().toString());

  mqttClient.begin("192.168.1.10", wifiClient);
  mqttConnect();

  ArduinoOTA.setHostname(HOSTNAME);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  Serial.println("ArduinoOTA ready");
}

void mqttConnect() {
  Serial.println("Connecting to MQTT...");
  while (!mqttClient.connect(HOSTNAME)) {
    Serial.print(".");
    delay(1000);
  }
}

unsigned long prevMillis = 0;
unsigned long prevMillisPMS = 0;
bool isPMSAwake = true;
bool isWet = false;

void loop() {
  if (WiFi.status() != WL_CONNECTED) return;
  mqttClient.loop();
  delay(10);
  ArduinoOTA.handle();

  if (!mqttClient.connected()) {
    mqttConnect();
  }

  int sensorValue = analogRead(rainPin);

  unsigned long currentMillis = millis();
  bool wet = sensorValue < thresholdValue;

  if (currentMillis - prevMillis >= 2000) {
    char wetData[150];
    sprintf(wetData, "{\"wet\": %s }", wet ? "true" : "false");
    mqttClient.publish("corner-end/rain", wetData);
    
    prevMillis = currentMillis;
    Serial.print(sensorValue);
    Serial.print(" isWet=");
    Serial.print(wet);
    char rainSensorData[150];
    sprintf(rainSensorData, "{\"value\": %d }", sensorValue);
    mqttClient.publish("corner-end/rain/sensorValue", rainSensorData);
    isWet = wet;

    // ESP Status
    char espStatus[150];
    sprintf(espStatus, "{\"uptime\": %d, \"rssi\": %d}", millis(), WiFi.RSSI());

    mqttClient.publish("corner-end/status", espStatus);

    Serial.print(" Temperature = ");
    Serial.print(bme.readTemperature());
    Serial.println(" *C");

    char bmeData[150];
    sprintf(bmeData, "{\"temp_c\": %f, \"pressure\": %f, \"altitude\": %f, \"humidity\": %f }", bme.readTemperature(), bme.readPressure() / 100.0F, bme.readAltitude(SEALEVELPRESSURE_HPA), bme.readHumidity());
    mqttClient.publish("corner-end/bme280", bmeData);
  }

  if (
    prevMillisPMS == 0 ||
    (!isPMSAwake && currentMillis - prevMillisPMS >= PMS_UPDATE_INTERVAL) ||
    (isPMSAwake && (currentMillis - prevMillisPMS >= PMS_READ_STABLE_DELAY))
  ) {
    prevMillisPMS = currentMillis;
    if (!isPMSAwake) {
      pms.wakeUp();
      isPMSAwake = true;
      Serial.println("PMS5003: Waking up");
    } else {
      Serial.println("PMS5003: Read data");
      pms.requestRead();

      if (pms.readUntil(data, 2000)) {
        char pmsData [150];
        sprintf(pmsData, "{\"pm1\": %d, \"pm2.5\": %d, \"pm10\": %d}", data.PM_AE_UG_1_0, data.PM_AE_UG_2_5, data.PM_AE_UG_10_0);
        mqttClient.publish("corner-end/pms5003", pmsData);
        Serial.println(pmsData);
      }

      pms.sleep();
      isPMSAwake = false;
      Serial.println("PMS5003: Sleep");
    }
  }
}

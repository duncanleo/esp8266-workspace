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

#define BME_READ_INTERVAL 60000

#define STATUS_INTERVAL 2000

#define PIN_RX  D5
#define PIN_TX  D6

Adafruit_BME280 bme;

WiFiClient wifiClient;
MQTTClient mqttClient(1024);

SoftwareSerial pmsSerial(PIN_RX, PIN_TX);
PMS pms(pmsSerial);
PMS::DATA data;


const char* kSsid = "";
const char* kPassword = "";

void setup() {
  Serial.begin(115200);

  // Setup BME280
  bool status = bme.begin(0x76);
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
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

  // HA discovery
  mqttClient.publish("homeassistant/sensor/corner_end_humidity/config", "{\"device_class\": \"humidity\",\"name\": \"Corner End Humidity\", \"unique_id\": \"corner_end_humidity\", \"state_topic\": \"corner-end/bme280\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ value_json.humidity }}\"}", true, 0);
  mqttClient.publish("homeassistant/sensor/corner_end_temp/config", "{\"device_class\": \"temperature\",\"name\": \"Corner End Temperature\", \"unique_id\": \"corner_end_temp\", \"state_topic\": \"corner-end/bme280\", \"unit_of_measurement\": \"°C\", \"value_template\": \"{{ value_json.temp_c }}\"}", true, 0);
  mqttClient.publish("homeassistant/sensor/corner_end_pressure/config", "{\"device_class\": \"pressure\",\"name\": \"Corner End Pressure\", \"unique_id\": \"corner_end_pressure\", \"state_topic\": \"corner-end/bme280\", \"unit_of_measurement\": \"Pa\", \"value_template\": \"{{ value_json.pressure }}\"}", true, 0);
  mqttClient.publish("homeassistant/sensor/corner_end_pm1/config", "{\"name\": \"Corner End PM1\", \"unique_id\": \"corner_end_pm1\", \"state_topic\": \"corner-end/pms5003\", \"unit_of_measurement\": \"μg/m³\", \"value_template\": \"{{ value_json.pm1 }}\"}", true, 0);
  mqttClient.publish("homeassistant/sensor/corner_end_pm10/config", "{\"name\": \"Corner End PM10\", \"unique_id\": \"corner_end_pm10\", \"state_topic\": \"corner-end/pms5003\", \"unit_of_measurement\": \"μg/m³\", \"value_template\": \"{{ value_json.pm10 }}\"}", true, 0);
  mqttClient.publish("homeassistant/sensor/corner_end_pm25/config", "{\"name\": \"Corner End PM2.5\", \"unique_id\": \"corner_end_pm25\", \"state_topic\": \"corner-end/pms5003\", \"unit_of_measurement\": \"μg/m³\", \"value_template\": \"{{ value_json.pm25 }}\"}", true, 0);

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

unsigned long prevMillisStatus = 0;
unsigned long prevMillisBME = 0;
unsigned long prevMillisPMS = 0;
bool isPMSAwake = true;

void loop() {
  if (WiFi.status() != WL_CONNECTED) return;
  mqttClient.loop();
  delay(10);
  ArduinoOTA.handle();

  if (!mqttClient.connected()) {
    mqttConnect();
  }

  unsigned long currentMillis = millis();

  if (currentMillis - prevMillisStatus >= STATUS_INTERVAL) {
    prevMillisStatus = currentMillis;

    // ESP Status
    char espStatus[150];
    sprintf(espStatus, "{\"uptime\": %d, \"rssi\": %d}", millis(), WiFi.RSSI());

    mqttClient.publish("corner-end/status", espStatus);
  }

  if (currentMillis - prevMillisBME >= BME_READ_INTERVAL) {
    prevMillisBME = currentMillis;    

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
        sprintf(pmsData, "{\"pm1\": %d, \"pm25\": %d, \"pm2.5\": %d, \"pm10\": %d}", data.PM_AE_UG_1_0, data.PM_AE_UG_2_5, data.PM_AE_UG_2_5, data.PM_AE_UG_10_0);
        mqttClient.publish("corner-end/pms5003", pmsData);
        Serial.println(pmsData);
      }

      pms.sleep();
      isPMSAwake = false;
      Serial.println("PMS5003: Sleep");
    }
  }
}

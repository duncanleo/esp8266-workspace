#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

int rainPin = A0;
int thresholdValue = 500;

void setup() {
  Serial.begin(9600);
  
  // setup Rain sensor
  pinMode(rainPin, INPUT);

  // Setup BME280
  bool status = bme.begin(0x76);
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  WiFi.begin("", "");

  Serial.print("Connecting to WiFi..");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Connected to the WiFi network");

  client.setServer("192.168.1.10", 1883);

  mqtt();
}

void mqtt() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");

    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

unsigned long prevMillis = 0;
bool singleForcePublish = true;
bool isWet = false;

void loop() {
  // put your main code here, to run repeatedly:

  bool connected = client.loop();

  if (!connected) {
    mqtt();
    singleForcePublish = true;
  }

  int sensorValue = analogRead(rainPin);

  unsigned long currentMillis = millis();
  bool wet = sensorValue < thresholdValue;

  if (wet != isWet || singleForcePublish) {
    char sbuf[150];
    sprintf(sbuf, "{\"wet\": %s }", wet ? "true" : "false");
    client.publish("corner-end/rain", sbuf);
  }

  if (currentMillis - prevMillis >= 2000) {
    prevMillis = currentMillis;
    Serial.print(sensorValue);
    Serial.print(" isWet=");
    Serial.print(wet);
    isWet = wet;

    Serial.print(" Temperature = ");
    Serial.print(bme.readTemperature());
    Serial.println(" *C");

    char sbuf[150];
    sprintf(sbuf, "{\"temp_c\": %f, \"pressure\": %f, \"altitude\": %f, \"humidity\": %f }", bme.readTemperature(), bme.readPressure() / 100.0F, bme.readAltitude(SEALEVELPRESSURE_HPA), bme.readHumidity());
    client.publish("corner-end/bme280", sbuf);
  }
  
  singleForcePublish = false;
}

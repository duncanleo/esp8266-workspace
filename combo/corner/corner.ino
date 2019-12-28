#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <Adafruit_HTU21DF.h>
#include <ArduinoOTA.h>
#include <MQTT.h>

#define HOSTNAME "esp-corner"

const uint16_t kIrLed = 14;  // ESP GPIO pin to use. Recommended: 4 (D2).

const char* kSsid = "";
const char* kPassword = "";

WiFiClient espClient;
MQTTClient mqttClient(2048);

Adafruit_HTU21DF htu;

IRsend irsend(kIrLed);  // Set the GPIO to be used to sending the message.

unsigned long prevMillis = 0;

void setup(void) {
  htu.begin();
  irsend.begin();

  Serial.begin(115200);
  WiFi.begin(kSsid, kPassword);
  WiFi.hostname(HOSTNAME);
  Serial.println("");

  ArduinoOTA.setHostname(HOSTNAME);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(kSsid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP().toString());

  mqttClient.begin("192.168.1.10", espClient);
  mqttClient.onMessage(callback);
  mqttConnect();

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

  Serial.println("HTTP server started");
}

void mqttConnect() {
  Serial.println("Connecting to MQTT...");
  while (!mqttClient.connect(HOSTNAME)) {
    Serial.print(".");
    delay(1000);
  }
  mqttClient.subscribe("corner/ir");
}

void loop(void) {
  if (WiFi.status() != WL_CONNECTED) return;
  mqttClient.loop();
  delay(10);
  ArduinoOTA.handle();

  if (!mqttClient.connected()) {
    mqttConnect();
  }

  unsigned long currentMillis = millis();
  if (currentMillis - prevMillis >= 2000) {
    prevMillis = currentMillis;

    // ESP Status
    char espStatus[150];
    sprintf(espStatus, "{\"uptime\": %d, \"rssi\": %d}", millis(), WiFi.RSSI());

    mqttClient.publish("corner/status", espStatus);

    // HTU21D
    float t = htu.readTemperature();
    float h = htu.readHumidity();
  
    char htu[150];
    sprintf(htu, "{\"temp_c\": %f, \"humidity\": %f}", t, h);

    mqttClient.publish("corner/htu21d", htu);
  }
}

void callback(String &topic, String &payload) {
  parseStringAndSendRaw(&irsend, payload);
}

// Count how many values are in the String.
// Args:
//   str:  String containing the values.
//   sep:  Character that separates the values.
// Returns:
//   The number of values found in the String.
uint16_t countValuesInStr(const String str, char sep) {
  int16_t index = -1;
  uint16_t count = 1;
  do {
    index = str.indexOf(sep, index + 1);
    count++;
  } while (index != -1);
  return count;
}

// Dynamically allocate an array of uint16_t's.
// Args:
//   size:  Nr. of uint16_t's need to be in the new array.
// Returns:
//   A Ptr to the new array. Restarts the ESP if it fails.
uint16_t * newCodeArray(const uint16_t size) {
  uint16_t *result;

  result = reinterpret_cast<uint16_t*>(malloc(size * sizeof(uint16_t)));
  // Check we malloc'ed successfully.
  if (result == NULL)  // malloc failed, so give up.
    ESP.restart();
//    doRestart(
//        "FATAL: Can't allocate memory for an array for a new message! "
//        "Forcing a reboot!", true);  // Send to serial only as we are in low mem
  return result;
}

#if SEND_RAW
// Parse an IRremote Raw Hex String/code and send it.
// Args:
//   irsend: A ptr to the IRsend object to transmit via.
//   str: A comma-separated String containing the freq and raw IR data.
//        e.g. "38000,9000,4500,600,1450,600,900,650,1500,..."
//        Requires at least two comma-separated values.
//        First value is the transmission frequency in Hz or kHz.
// Returns:
//   bool: Successfully sent or not.
bool parseStringAndSendRaw(IRsend *irsend, const String str) {
  uint16_t count;
  uint16_t freq = 38000;  // Default to 38kHz.
  uint16_t *raw_array;

  // Find out how many items there are in the string.
  count = countValuesInStr(str, ',');

  // We expect the frequency as the first comma separated value, so we need at
  // least two values. If not, bail out.
  if (count < 2)  return false;
  count--;  // We don't count the frequency value as part of the raw array.

  // Now we know how many there are, allocate the memory to store them all.
  raw_array = newCodeArray(count);

  // Grab the first value from the string, as it is the frequency.
  int16_t index = str.indexOf(',', 0);
  freq = str.substring(0, index).toInt();
  uint16_t start_from = index + 1;
  // Rest of the string are values for the raw array.
  // Now convert the strings to integers and place them in raw_array.
  count = 0;
  do {
    index = str.indexOf(',', start_from);
    raw_array[count] = str.substring(start_from, index).toInt();
    start_from = index + 1;
    count++;
  } while (index != -1);

  Serial.println(count);

  irsend->sendRaw(raw_array, count, freq);  // All done. Send it.
  free(raw_array);  // Free up the memory allocated.
  if (count > 0)
    return true;  // We sent something.
  return false;  // We probably didn't.
}
#endif  // SEND_RAW

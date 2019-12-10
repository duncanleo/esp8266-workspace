#include <Adafruit_HTU21DF.h>

Adafruit_HTU21DF htu;

void setup() {
  Serial.begin(9600);
  htu.begin();
}

void loop() {
  Serial.println("Temp:");
  Serial.println(htu.readTemperature());
  Serial.println("Hum:");
  Serial.println(htu.readHumidity());
  delay(2000);
}

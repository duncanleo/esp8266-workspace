#include "DHTesp.h"

#define DHTPIN 2

DHTesp dht;

void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);
  Serial.setTimeout(2000);

  while (!Serial) {}

  dht.setup(DHTPIN, DHTesp::AM2302);

  Serial.println("Device started");
  Serial.println("==============");
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(dht.getMinimumSamplingPeriod());
  float h = dht.getHumidity();
  float t = dht.getTemperature();

  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print(dht.getStatusString());
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.print(" *C ");
  Serial.println("");

  delay(2000);
}

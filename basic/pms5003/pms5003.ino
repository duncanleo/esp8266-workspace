#include "PMS.h"
#include <SoftwareSerial.h>

#define PIN_RX  D1
#define PIN_TX  D2

SoftwareSerial ser(PIN_RX, PIN_TX);
PMS pms(ser);
PMS::DATA data;

void setup() {
  Serial.begin(115200);
  Serial.println("PMS");

  ser.begin(9600);
}

auto lastRead = millis();

void loop() {
  if (pms.read(data))
  {
    Serial.print("PM 1.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_1_0);

    Serial.print("PM 2.5 (ug/m3): ");
    Serial.println(data.PM_AE_UG_2_5);

    Serial.print("PM 10.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_10_0);

    Serial.println();
  }
}

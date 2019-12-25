/*
 
 All the resources for this project:
 https://randomnerdtutorials.com/
 
*/

int rainPin = A0;
// you can adjust the threshold value
int thresholdValue = 500;

void setup(){
  pinMode(rainPin, INPUT);
  Serial.begin(9600);
}

void loop() {
  // read the input on analog pin 0:
  int sensorValue = analogRead(rainPin);
  Serial.print(sensorValue);
  if(sensorValue < thresholdValue){
    Serial.println(" - It's wet");
  }
  else {
    Serial.println(" - It's dry");
  }
  delay(500);
}

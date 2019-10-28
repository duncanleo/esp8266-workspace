# pms5003
This is a script that exposes a web server on port 80. It exposes a PMS5003 sensor through a JSON API.

As the library used supports `SoftwareSerial`, the USB serial console works while debugging this script.

#### ESP8266 NodeMCU ESP-12E
The script is configured for the PMS5003 RX to be plugged into D1 and the TX to be plugged into D2.

### Dependencies
- https://github.com/riverscn/pmsx003 (add this library via the Zip file option in Arduino IDE)

API usage:

`GET /wake`: Wake the PMS5003 sensor  
`GET /read`: Read data from the PMS5003 sensor  
Sample response:
```json
{
  "status": false,
  "message": "ok",
  "data": {
    "pm1": 42,
    "pm25": 43,
    "pm10": 47
  }
}
```
`GET /sleep`: Hibernate the PMS5003 sensor

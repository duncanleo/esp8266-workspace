# ir
The code in this repository are for IR transmitters and receivers.

All images and code adapted from https://github.com/crankyoldgit/IRremoteESP8266.

`learn.ino` allows you to connect an IR receiver and learn raw IR data from a remote. The raw code returned from this script can be prepended with `38` (its IR transmission rate) and used with `server.ino`.

`server.ino` exposes a web server on port 80 with the following API:

`GET /ir?code=<rate>,<raw1>,<raw2>,...` - Transmit raw code. Rate is transmit rate (common values are `38`) and the rest of the comma-separated values are the raw IR data.

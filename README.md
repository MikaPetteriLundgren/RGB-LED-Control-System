RGB-LED-Control-System
=================

ESP-8266 based RGB LED control implementation for Domoticz home automation system which controls colours and brightness of the RGB LED stripe.
The sketch includes the following features:

* Receives MQTT messages from Domoticz and parses JSON data out of the MQTT messages
* Controls the RGB LED stripe based on the messages received from the Domoticz

ESP-8266 is connected to local network with Wifi and listens "domoticz/out" MQTT topic for incoming messages. Domoticz is using JSON format in the MQTT messages.
Memory for incoming MQTT messages is allocated from heap based on the size of the received payload.

IDX value is parsed out from incoming MQTT messages. If the parsed IDX value matches with the predefined IDX value, rest of the needed values 
to control the RGB LED stripe are parsed out.

Colour and brightness of RGD LED stripe is set based on the values received from the Domoticz. The sketch doesn't send any data to the Domoticz via MQTT.

If the MQTT connection has been disconnected, reconnection will take place. If the reconnection fails 10 times, Wifi connection is disconnected and initialized again.

It's possible to print amount of free RAM memory via serial port by uncommenting `#define RAM_DEBUG` line
It's possible to print more debug information via serial port by uncommenting `#define DEBUG` line

The sketch will need following HW and SW libraries to work:

**HW**

* ESP-8266 based MCU (Lolin D1 mini used in this implementation)
* Level shifter
* RGB LED stripe (WS2813 based stripe used in this implementation)

**Libraries**

* PubSubClient for MQTT communication
* ArduinoJson for JSON message parsing
* NeoPixelBrightnessBus for RGB LED stripe control

**HW connections**

The Lolin D1 mini is connected to the WS2813 RGB LED stripe using level shifter. The level shifter is needed because the Lolin D1 mini is
working in 3.3V whereas the WS2813 is working in 5.0V. 

This repository includes schematics of the system.

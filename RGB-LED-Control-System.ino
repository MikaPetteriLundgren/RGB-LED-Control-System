/*
   The sketch is implemented for Lolin D1 Mini powered by ESP-8266EX MCU.
   The sketch receives JSON messages from Domoticz home automation system and parses IDX value out of each and every message.
   If the parsed IDX value matches with the predefined IDX value, rest of the needed values are parsed out.

   Colour and brightness of RGD LED stripe is set based on the values received from the Domoticz. The sketch sends log message to Domoticz after changing RGB LED stripe's settings.

   The sketch needs RGB-LED-Control-System.h header file in order to work. The header file includes settings for the sketch.
   */

#include "/Users/Miksu/Documents/Arduino/Projektit/RGB-LED-Control-System/RGB-LED-Control-System.h" // Header file containing settings for the sketch
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NeoPixelBrightnessBus.h>

// Wifi network settings
char ssid[] = NETWORK_SSID; // Network SSID (name) is defined in the header file
char pass[] = NETWORK_PASSWORD; // Network password is defined in the header file
int status = WL_IDLE_STATUS; // Wifi status
IPAddress ip; // IP address
WiFiClient wifiClient; // Initialize Wifi Client
byte mac[6]; // MAC address of Wifi module
byte WiFiTimeout = 30; // Wi-Fi connection timeout in seconds. System to be rebooted if the timeout expires.

// MQTT callback function header
void mqttCallback(char* topic, byte* payload, unsigned int length);

//MQTT initialization
PubSubClient mqttClient(MQTT_SERVER, 1883, mqttCallback, wifiClient); // MQTT_SERVER constructor parameter is defined in the header
char clientID[50];
char msg[80];
unsigned int MQTTBufferSize = 512; // Size on internal MQTT buffer for incoming/outgoing messages in bytes. Default value is 256B which is not big enough for this use case
char topic[] = MQTT_TOPIC; // Topic for outgoing MQTT messages to the Domoticz. The MQTT_TOPIC is defined in the header file
char subscribeTopic[ ] = MQTT_SUBSCRIBE_TOPIC; // Arduino will listen this topic for incoming MQTT messages from the Domoticz. The MQTT_SUBSCRIBE_TOPIC is defined in the header file
byte MQTTtimeout = 30; // MQTT connection timeout in seconds. System to be rebooted if the timeout expires.

//MQTT variables
int receivedIDX = 0; // Received IDX number will be stored to this variable
int switchIDX = 1434; // IDX of the RGB switch controlling the RGB LEDs in Domoticz

//WS2813 RGB LED stripe and NeoPixel library initialization
const uint16_t PixelCount = 76; // 76 RGB LEDs on the stripe
const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266
//NeoPixelBrightnessBus<NeoRgbFeature, NeoWs2813Method> strip(PixelCount, PixelPin); // Use with Arduino
NeoPixelBrightnessBus<NeoRgbFeature, NeoEsp8266Uart1Ws2813Method> strip(PixelCount, PixelPin); // Use with Lolin D1 Mini
#define colorSaturation 255
unsigned int RGB_R = 0; // variable to store value of red component (0-255)
unsigned int RGB_G = 0; // variable to store value of green component (0-255)
unsigned int RGB_B = 0; // variable to store value of blue component (0-255)
unsigned int brightness = 100; // variable to store value of brightness (0-100)
unsigned int RGBLED_state = 0; // variable to store information whether RGD LEDs are turned on (>0) or off (0)
String deviceName = "";
RgbColor receivedColor(RGB_G, RGB_R, RGB_B);
RgbColor white(colorSaturation);
RgbColor black(0);

//#define DEBUG // Comment this line out if there is no need to print debug information via serial port
//#define RAM_DEBUG // Comment this line out if there is no need to print RAM debug information via serial port
//#define WIFI_DEBUG // Comment this line out if there is no need to print WIFI debug information via serial port


void setup()
{
  Serial.begin(115200); // Start serial port
  //while (!Serial) ; // Needed with Arduino Leonardo only

  Serial.println(F("\n---------------- Setup started ----------------\n"));

  //Print MAC address
  Serial.print(F("Wi-Fi MAC address: "));
  Serial.println(WiFi.macAddress());

  // Start Wifi
  startWiFi();
  WiFi.macAddress(mac);

  // MQTT client ID is constructed from the MAC address in order to be unique. The unique MQTT client ID is needed in order to avoid issues with Mosquitto broker
  String clientIDStr = "Lolin_D1_Mini_";
  clientIDStr.concat(WiFi.macAddress());

  Serial.print(F("MQTT client ID: "));
  Serial.println(clientIDStr);
  clientIDStr.toCharArray(clientID, clientIDStr.length()+1);

  //Size of an internal MQTT buffer is increased and MQTT connection is established and topic subscribed for the callback function.
  Serial.print(F("Size of an internal MQTT buffer set to "));
  Serial.print(MQTTBufferSize);
  mqttClient.setBufferSize(MQTTBufferSize) ? Serial.println(F("B succesfully")) : Serial.println(F("B failed. Default 256B to be used...")); //condition ? valueIfTrue : valueIfFalse

  mqttClient.connect(clientID) ? Serial.println(F("MQTT client connected")) : Serial.println(F("MQTT client connection failed...")); //condition ? valueIfTrue : valueIfFalse
  Serial.print(F("MQTT topic "));
  Serial.print(subscribeTopic);
  mqttClient.subscribe(subscribeTopic) ? Serial.println(F(" subscribed succesfully")) : Serial.println(F(" subscription failed...")); //condition ? valueIfTrue : valueIfFalse
  
  //Reset all neopixels to off state
  strip.Begin();
  strip.Show();

  Serial.println(F("\n---------------- Setup completed succesfully ----------------\n"));
}


void loop()
{
    //If connection to MQTT broker is disconnected. Connect and subscribe again
    if (!mqttClient.connected())
    {
      Serial.println(F("MQTT client disconnected"));
      printMQTTClientState(mqttClient.state()); // print the state of the client
      Serial.println(F("Trying to reconnect"));
      byte timeoutCounter = 0;

      while (!mqttClient.connect(clientID))
      {
        delay(500);
        Serial.print(".");
        timeoutCounter += 1;

        if (timeoutCounter >= (MQTTtimeout * 2));
        {
          Serial.println("MQTT connection timeout. Wi-Fi to be reconnected.");
          WiFi.disconnect(); // Disconnect from the Wi-Fi network
          delay(5000);
          startWiFi(); // Connect to the Wi-Fi network
          timeoutCounter = 0;
        }
      }
    
      Serial.println(F("MQTT client connected"));

      if (mqttClient.subscribe(subscribeTopic)) //MQTT topic subscribed for the callback function
      {
        Serial.println(F("MQTT topic subscribed succesfully"));
      }
      else
      {
        Serial.println(F("MQTT topic subscription failed"));
        mqttClient.disconnect(); // Disconnect the MQTT client
      }
    }

    mqttClient.loop(); //This should be called regularly to allow the MQTT client to process incoming messages and maintain its connection to the server.

   #if defined RAM_DEBUG
     Serial.print(F("Amount of free RAM memory: "));
     Serial.print(ESP.getFreeHeap()/1024); // Returned value is in Bytes which is converted to kB
     Serial.println(F("kB")); //ESP-8266EX has 2kB of RAM memory
   #endif
}

void startWiFi() //ESP8266 Wifi
{
  Serial.printf("Wi-Fi set to station mode (WIFI_STA) %s\n", WiFi.mode(WIFI_STA) ? "" : "failed!"); // Set ESP8266 to a client mode
  WiFi.begin(ssid, pass);
  byte timeoutCounter = 0;

  Serial.print("Wi-Fi connection timeout set to ");
  Serial.print(WiFiTimeout);
  Serial.println("s");
  Serial.print("Connecting to Wi-Fi network ");
  Serial.print(ssid);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");

    if (timeoutCounter >= WiFiTimeout * 2)
    {
      printWifiState(WiFi.status());
      Serial.println("\nWi-fi connection timeout");
      Serial.println("System to be restarted in 2s...");
      delay(2000);
      ESP.restart();
    }
    timeoutCounter += 1;
  }

  Serial.println();
  Serial.print(F("Connected to WiFi network: "));
  Serial.println(ssid);
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());
  Serial.print(F("RSSI: "));
  Serial.print(WiFi.RSSI());
  Serial.println(F("dBm"));

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}


// mqttCallback function handles messages received from subscribed MQTT topic(s)
void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  Serial.print(F("MQTT message arrived ["));
  Serial.print(topic);
  Serial.print("] ");

  #if defined DEBUG
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  Serial.println();
  Serial.print(F("Size of the payload is: "));
  Serial.print(length);
  Serial.println();
  #endif

  if(jsonParser(payload, length))
  {
    Serial.println(F("JSON parsing succesful\n"));
  }
  else
  {
    Serial.println(F("JSON parsing failed\n"));
  }
}


bool jsonParser(byte* dataPayload, unsigned int dataLength) // Parse received JSON document. Return true if parsing succeeds and false if it fails
{
  DynamicJsonDocument doc(dataLength); // Allocate memory from heap based on the size of the received payload
  DeserializationError error = deserializeJson(doc, dataPayload); // Deserialize the JSON document
  
  // Test if JSON parsing succeeds
  if(error)
  {
    
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
   }

  // Fetch IDX from the JSON document
  receivedIDX = doc["idx"];

  // Print fetched IDX value
    Serial.print(F("Parsed IDX:"));
    Serial.println(receivedIDX);

  if(receivedIDX == switchIDX) // JSON parsing to be finished only if the fetched IDX matches the predefined IDX
  {
    Serial.println(F("Correct IDX received!"));
    RGB_R = doc["Color"]["r"];
    RGB_G = doc["Color"]["g"];
    RGB_B = doc["Color"]["b"];
    brightness = doc["Level"];
    RGBLED_state = doc["nvalue"];
    const char* parsedName = doc["name"];
    Serial.println(F("Parsed RGB LED values:"));
    Serial.print(F("RGB_R: "));
    Serial.println(RGB_R);
    Serial.print(F("RGB_G: "));
    Serial.println(RGB_G);
    Serial.print(F("RGB_B: "));
    Serial.println(RGB_B);
    Serial.print(F("Brightness: "));
    Serial.println(brightness);
    Serial.print(F("RGB LEDs On/Off: "));
    (RGBLED_state == 0) ? Serial.println(F("Off")) : Serial.println(F("On")); //condition ? valueIfTrue : valueIfFalse

    RGBLEDControl(RGB_R, RGB_G, RGB_B, brightness, RGBLED_state);

    deviceName = String(parsedName); // parsedName is converted to String
    sendMQTTPayload(createMQTTPayload(deviceName, switchIDX)); // Send a message to Domoticz's log
   }

  return true;
}


String createMQTTPayload(String name, int idx) //Create MQTT message payload. Returns created payload as a String.
{
  StaticJsonDocument<100> doc; // Capacity has been calculated in arduinojson.org/v6/assistant

  // Create a message to be sent to Domoticz log
  String payloadStr = name;
  payloadStr.concat(" (IDX ");
  payloadStr.concat(idx);
  payloadStr.concat(") set to ");
  payloadStr.concat(RGBLED_state == 0 ? "Off" : "On");

  // Add values in the JSON document
  doc["command"] = "addlogmessage";
  doc["message"] = payloadStr;

  // Generate the prettified JSON and store it to the string
  String dataMsg = "";
  serializeJsonPretty(doc, dataMsg);

	return dataMsg;
}


void sendMQTTPayload(String payload) // Sends MQTT payload to the MQTT broker / Domoticz
{

	// Convert payload to char array
	payload.toCharArray(msg, payload.length()+1);

    //If connection to MQTT broker is disconnected, connect again
  if (!mqttClient.connected())
  {
    (mqttClient.connect(clientID)) ? Serial.println(F("MQTT client connected")) : Serial.println(F("MQTT client connection failed...")); //condition ? valueIfTrue : valueIfFalse - This is a Ternary operator
  }

	//Publish payload to MQTT broker
	if (mqttClient.publish(topic, msg))
	{
		Serial.println(F("Following data published to MQTT broker: "));
		Serial.print(F("Topic: "));
    Serial.println(topic);
		Serial.println(payload);
		Serial.println();
	}
	else
  {
		Serial.println(F("Publishing to MQTT broker failed"));
  }
}


void RGBLEDControl(unsigned int red, unsigned int green, unsigned int blue, unsigned int stripeBrightness, unsigned int stripeStatus) // Controls RGB LED stripe based on the received colour and brightness data
{
  if (stripeStatus == 0)
  {
    Serial.println(F("Turning RGB LEDs off..."));
    for (int i = 0; i < PixelCount; i++)
    {
      strip.SetPixelColor(i, black); // Turn pixels off
    }

    strip.Show(); // Apply changes

  }
  else
  {
    Serial.println(F("Applying received colour and brightness settings to the RGB LED stripe"));

    RgbColor receivedColor(green, red, blue);

    for (int i = 0; i < PixelCount; i++) 
    {
      strip.SetPixelColor(i, receivedColor); // Set colour
    }
    strip.SetBrightness(255 * stripeBrightness / 100); // Set brightness
    strip.Show(); // Apply changes
  }
  
  
}


void printMQTTClientState(int MQTTClientState) // Prints MQTT client state in human readable format. The states can be found from here: https://pubsubclient.knolleary.net/api.html#state
{

  Serial.print(F("State of the MQTT client: "));
  Serial.print(MQTTClientState);
  
  switch (MQTTClientState)
  {
   case -4:
     Serial.println(F(" - the server didn't respond within the keepalive time")); 
     break;
   case -3:
     Serial.println(F(" - the network connection was broken")); 
     break;
   case -2:
     Serial.println(F(" - the network connection failed")); 
     break;
   case -1:
     Serial.println(F(" - the client is disconnected cleanly")); 
     break;
   case 0:
     Serial.println(F(" - the client is connected")); 
     break;
   case 1:
     Serial.println(F(" - the server doesn't support the requested version of MQTT")); 
     break;
   case 2:
     Serial.println(F(" - the server rejected the client identifier")); 
     break;
   case 3:
     Serial.println(F(" - the server was unable to accept the connection")); 
     break;
   case 4:
     Serial.println(F("  - the username/password were rejected")); 
     break;
   case 5:
     Serial.println(F(" - the client was not authorized to connect")); 
     break;   
   default:
     Serial.println(F(" - unknown state")); 
     break;
  }  
}


void printWifiState(int WifiStatus) // Prints Wi-Fi client status in human readable format. The statuses can be found via the following link: https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html
{

  Serial.print(F("\nWi-Fi status: "));
  Serial.print(WifiStatus);
  
  switch (WifiStatus)
  {
   case 0:
     Serial.println(F(" - Wi-Fi client is in process of changing states")); 
     break;
   case 1:
     Serial.println(F(" - Wi-Fi network not found")); 
     break;
   case 3:
     Serial.println(F(" - Connected")); 
     break;
   case 4:
     Serial.println(F(" - Connection failed")); 
     break;
   case 6:
     Serial.println(F(" - Password is incorrect")); 
     break;
   case 7:
     Serial.println(F(" - Module is not configured in station mode")); 
     break;   
   default:
     Serial.println(F(" - Unknown Wi-Fi status code received")); 
     break;
  }  

  // Wi-Fi debug info is printed out if WIFI_DEBUG is uncommented
  #if defined WIFI_DEBUG
    Serial.println(F("Wi-Fi diagnostics info:"));
    Serial.setDebugOutput(true);
    WiFi.printDiag(Serial);
  #endif

}

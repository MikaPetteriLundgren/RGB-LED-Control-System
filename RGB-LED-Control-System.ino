/*
   The sketch is implemented for Lolin D1 Mini powered by ESP-8266EX MCU.
   The sketch receives JSON messages from Domoticz home automation system and parses IDX value out of each and every message.
   If the parsed IDX value matches with the predefined IDX value, rest of the needed values are parsed out.

   Colour and brightness of RGD LED stripe is set based on the values received from the Domoticz. The sketch doesn't send any data to the Domoticz via MQTT.

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

// MQTT callback function header
void mqttCallback(char* topic, byte* payload, unsigned int length);

//MQTT initialization
PubSubClient mqttClient(MQTT_SERVER, 1883, mqttCallback, wifiClient); // MQTT_SERVER constructor parameter is defined in the header file
char clientID[50];
char topic[50];
char msg[80];
char subscribeTopic[ ] = MQTT_SUBSCRIBE_TOPIC; // Arduino will listen this topic for incoming MQTT messages from the Domoticz. The MQTT_SUBSCRIBE_TOPIC is defined in the header file
int mqttConnectionFails = 0; // If MQTT connection is disconnected for some reason, this variable is increment by 1

//MQTT variables
int receivedIDX = 0; // Received IDX number will be stored to this variable
int switchIDX = 1469; // IDX of the RGB switch controlling the RGB LEDs in Domoticz

//WS2813 RGB LED stripe and NeoPixel library initialization
const uint16_t PixelCount = 76; // 76 RGB LEDs on the stripe
const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266
//NeoPixelBrightnessBus<NeoRgbFeature, NeoWs2813Method> strip(PixelCount, PixelPin); // Use with Arduino
NeoPixelBrightnessBus<NeoRgbFeature, NeoEsp8266Uart1800KbpsMethod> strip(PixelCount, PixelPin); // Use with Lolin D1 Mini
#define colorSaturation 255
unsigned int RGB_R = 0; // variable to store value of red component (0-255)
unsigned int RGB_G = 0; // variable to store value of green component (0-255)
unsigned int RGB_B = 0; // variable to store value of blue component (0-255)
unsigned int brightness = 100; // variable to store value of brightness (0-100)
unsigned int RGBLED_state = 0; // variable to store information whether RGD LEDs are turned on (>0) or off (0)
RgbColor receivedColor(RGB_G, RGB_R, RGB_B);
RgbColor white(colorSaturation);
RgbColor black(0);

//#define DEBUG // Comment this line out if there is no need to print debug information via serial port
//#define RAM_DEBUG // Comment this line out if there is no need to print RAM debug information via serial port


void setup()
{
  Serial.begin(115200); // Start serial port
  //while (!Serial) ; // Needed with Arduino Leonardo only

  // Start Wifi
  startWiFi();
  WiFi.macAddress(mac);

  //Create MQTT client String
  Serial.print(F("MAC address: "));
  for (int i = (sizeof(mac)-1); i>=0; i--)
  {
    Serial.print(mac[i],HEX);
    Serial.print(":");
  }
  Serial.println();

  // MQTT client ID is constructed from the MAC address in order to be unique. The unique MQTT client ID is needed in order to avoid issues with Mosquitto broker
  String clientIDStr = "Lolin_D1_Mini_";

  for (int i = (sizeof(mac)-1); i>=0; i--)
  {
    clientIDStr.concat(mac[i]);
  }

  Serial.print(F("MQTT client ID: "));
  Serial.println(clientIDStr);
  clientIDStr.toCharArray(clientID, clientIDStr.length()+1);

  //MQTT connection is established and topic subscribed for the callback function
  mqttClient.connect(clientID) ? Serial.println(F("MQTT client connected")) : Serial.println(F("MQTT client connection failed...")); //condition ? valueIfTrue : valueIfFalse
  mqttClient.subscribe(subscribeTopic) ? Serial.println(F("MQTT topic subscribed succesfully")) : Serial.println(F("MQTT topic subscription failed...")); //condition ? valueIfTrue : valueIfFalse

  //Reset all neopixels to off state
  strip.Begin();
  strip.Show();

  Serial.println(F("Setup completed succesfully!\n"));
}


void loop()
{
    //If connection to MQTT broker is disconnected. Connect and subscribe again
    if (!mqttClient.connected())
    {
      printClientState(mqttClient.state()); // print the state of the client
      
      (mqttClient.connect(clientID)) ? Serial.println(F("MQTT client connected")) : Serial.println(F("MQTT client connection failed...")); //condition ? valueIfTrue : valueIfFalse

      //MQTT topic subscribed for the callback function
      (mqttClient.subscribe(subscribeTopic)) ? Serial.println(F("MQTT topic subscribed succesfully")) : Serial.println(F("MQTT topic subscription failed...")); //condition ? valueIfTrue : valueIfFalse

      mqttConnectionFails +=1; // If MQTT connection is disconnected for some reason this variable is increment by 1

      // Wifi connection to be disconnected and initialized again if MQTT connection has been disconnected 10 times
      if (mqttConnectionFails >= 10)
      {
        Serial.println(F("Wifi connection to be disconnected and initialized again!"));
        WiFi.disconnect();
        mqttConnectionFails = 0;
        startWiFi();
      }
    }

    mqttClient.loop(); //This should be called regularly to allow the MQTT client to process incoming messages and maintain its connection to the server.

    delay(1000);

   #if defined RAM_DEBUG
     Serial.print(F("Amount of free RAM memory: "));
     Serial.print(ESP.getFreeHeap()/1024); // Returned value is in Bytes which is converted to kB
     Serial.println(F("kB")); //ESP-8266EX has 2kB of RAM memory
   #endif
}

void startWiFi() //ESP8266 Wifi
{
  WiFi.begin(ssid, pass);

  Serial.print("Connecting to Wi-Fi network ");
  Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
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

  // Print fetched IDX value if Debug mode is enabled
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
   }

  return true;
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

void printClientState(int clientState) // Prints MQTT client state in human readable format. The states can be found from here: https://pubsubclient.knolleary.net/api.html#state
{

  Serial.print(F("State of the MQTT client: "));
  Serial.print(clientState);
  
  switch (clientState)
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

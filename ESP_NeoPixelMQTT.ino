
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>

#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <DataToMaker.h>
#include <ClickButton.h>

#include <PubSubClient.h>

#define DEBUG 1

#ifdef DEBUG
#define DEBUG_PRINT(x)      Serial.print (x)
#define DEBUG_PRINTDEC(x,DEC) Serial.print (x, DEC)
#define DEBUG_PRINTLN(x)    Serial.println (x)
#define DEBUG_PRINTLNDEC(x,DEC) Serial.println (x, DEC)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x,DEC)
#define DEBUG_PRINTLN(x) 
#define DEBUG_PRINTLNDEC(x,DEC)
#endif

#define MQTT_MAX_PACKET_SIZE 256


const char* mqtt_server = "192.168.6.106";

WiFiClient espClient;
PubSubClient _mqttClient(espClient);
String _lastMQTTMessage = "";
const char* _espHostName="neo";

#define NUMPIXELS      8
// Which pin on the Arduino is connected to the NeoPixels?
#define PIXELPIN D4

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);

int r=0, g=0, b=0;

void mqttPublish(String name, String value){
  DEBUG_PRINTLN("MQTT Sending ");
  String __ret = ("stat/" + String(_espHostName) + "/" + name);
  String __retVal = value;
  DEBUG_PRINTLN(__ret + " " + __retVal);
  _mqttClient.publish(__ret.c_str(), __retVal.c_str());
  DEBUG_PRINTLN("MQTT: Sent");
}

void mqttReconnect() {
  String __clientId = String(_espHostName);
  __clientId += String(random(0xffff), HEX);
  DEBUG_PRINTLN("MQTT reconnecting");
  int __retryCount = 0;
  while (!_mqttClient.connected() && __retryCount < 3) {
    if (_mqttClient.connect(__clientId.c_str())) {
      DEBUG_PRINTLN("MQTT: Connected");

      DEBUG_PRINTLN("MQTT: Setting callback");
      _mqttClient.setCallback(mqttCallback);

      // Once connected, publish an announcement...
      String __outMessage = "tele/" + String(_espHostName) + "/alive";
      _mqttClient.publish(__outMessage.c_str(), "1");
      __outMessage = "tele/" + String(_espHostName) + "/ip";
      _mqttClient.publish(__outMessage.c_str(), IpAddress2String(WiFi.localIP()).c_str());

      DEBUG_PRINTLN("MQTT: subscribing...");
      delay(1000);
      String __topic = "cmnd/neo/#";
      _mqttClient.subscribe(__topic.c_str());

      _mqttClient.loop();
    }
    else {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINT(_mqttClient.state());
      DEBUG_PRINTLN(" try again in a few seconds");
      // Wait a few seconds before retrying
      delay(1500);
      __retryCount++;
    }
  }
}

void mqttTransmitStat(){

}

void mqttSendStat()
{
  if (!_mqttClient.connected()) { mqttReconnect(); }
  //mqttTransmitStat(); 
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  DEBUG_PRINT("Message arrived [");
  DEBUG_PRINT(topic);
  DEBUG_PRINT("] ");
  _lastMQTTMessage= "Message arrived";

  char message_buff[MQTT_MAX_PACKET_SIZE];
  int i = 0;
  for (i = 0; i < length; i++) { message_buff[i] = payload[i]; }
  message_buff[i] = '\0';
  String __payloadString = String(message_buff);

  __payloadString.trim();

  DEBUG_PRINTLN(__payloadString);

  String __incoming = String(topic);
  __incoming.trim();
  
  _lastMQTTMessage = __incoming +" " + __payloadString;

  if (__incoming == "cmnd/neo/r"){
    r = __payloadString.toInt();
  }
  if (__incoming == "cmnd/neo/g"){
    g =__payloadString.toInt();
  }  
   if (__incoming == "cmnd/neo/b"){
    b = __payloadString.toInt();
  }

}

String IpAddress2String(const IPAddress& ipAddress){
  return String(ipAddress[0]) + String(".") + \
    String(ipAddress[1]) + String(".") + \
    String(ipAddress[2]) + String(".") + \
    String(ipAddress[3]);
}

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif // DEBUG

  DEBUG_PRINTLN("   ");
  DEBUG_PRINTLN("Initialising");

  DEBUG_PRINT("Sketch size: ");
  DEBUG_PRINTLN(ESP.getSketchSize());
  DEBUG_PRINT("Free size: ");
  DEBUG_PRINTLN(ESP.getFreeSketchSpace());

  // If shield not present
  if (WiFi.status() == WL_NO_SHIELD) {

    DEBUG_PRINTLN("WiFi shield not present: Freezing");
    // Don't continue
    for (;;);
  }

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setDebugOutput(false);

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setMinimumSignalQuality(15);
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(360);

  //wifiManager.resetSettings();


  String __apName = String(_espHostName) + "_" + String(ESP.getChipId());
  DEBUG_PRINT("Wifi AP Mode. Broadcasting as:");
  DEBUG_PRINTLN(__apName);

  DEBUG_PRINTLN("Starting wifiManager");
  if (!wifiManager.autoConnect(__apName.c_str())) {
    DEBUG_PRINTLN("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  randomSeed(micros());

  DEBUG_PRINTLN("Attempting MQTT connection...");
  // Create a random client ID

  _mqttClient.setServer(mqtt_server, 1883);
  _mqttClient.setCallback(mqttCallback);

  mqttReconnect();
  //we know we are connected so force resend current status
  mqttTransmitStat();

  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("WiFi connected");

  // Print the IP address
  DEBUG_PRINTLN(WiFi.localIP());
  DEBUG_PRINTLN(WiFi.macAddress());
  
  MDNS.begin(_espHostName);
  
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
}

void loop() {

  if (!_mqttClient.connected()) { mqttReconnect(); }
  _mqttClient.loop();
  
  colorWipe(strip.Color(r, g, b), 50);
  delay(2000);
  
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}


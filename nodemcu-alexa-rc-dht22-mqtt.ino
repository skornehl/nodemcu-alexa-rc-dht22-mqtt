// Imports
#include <ESP8266WiFi.h>
#include "credentials.h"
#include <RCSwitch.h>         //https://github.com/sui77/rc-switch
#include "fauxmoESP.h"        //https://bitbucket.org/xoseperez/fauxmoesp
#include <MQTT.h>             //https://github.com/256dpi/arduino-mqtt

#include "DHT.h"
#include <ArduinoJson.h>

//https://www.losant.com/blog/getting-started-with-the-esp8266-and-dht22-sensor
// Globals
#define SERIAL_BAUDRATE                 115200
#define LED                             2

// DHT22
#define DHTPIN 14     // what digital pin the DHT22 is conected to
#define DHTTYPE DHT22   // there are multiple kinds of DHT sensors

DHT dht(DHTPIN, DHTTYPE);

// WEMO
#define room_kinderzimmer               "10001"

char* devices[][3] =
  // Format: RoomID, Name, ID
{
  { room_kinderzimmer, "10000", "Lichterschlauch" },
  { room_kinderzimmer, "01000", "Schloss" },
  { room_kinderzimmer, "00100", "Kleiderschrank" },
  { room_kinderzimmer, "00010", "Himmel" },
};

RCSwitch mySwitch = RCSwitch();
fauxmoESP fauxmo;

// MQTT
MQTTClient client;
WiFiClient net;


// -----------------------------------------------------------------------------
// Wifi
// -----------------------------------------------------------------------------

void wifiSetup() {

  // Set WIFI module to STA mode
  WiFi.mode(WIFI_STA);

  // Connect
  Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Wait
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Connected!
  Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

}

// -----------------------------------------------------------------------------
// 433MHz
// -----------------------------------------------------------------------------
void rfSetup() {

  // Set GPIO Pin
  mySwitch.enableTransmit(2);

  // Optional set protocol (default is 1, will work for most outlets)
  // mySwitch.setProtocol(1);

  // Optional set pulse length.
  //mySwitch.setPulseLength(120);

  // Optional set number of transmission repetitions.
  //mySwitch.setRepeatTransmit(5);
}

//https://learn.adafruit.com/easy-alexa-or-echo-control-of-your-esp8266-huzzah/software-setup
// -----------------------------------------------------------------------------
// callback makes it more generic
// -----------------------------------------------------------------------------
void callback(uint8_t device_id, const char * device_name, bool state) {
  digitalWrite(LED, 0);
  Serial.print("Room "); Serial.println(devices[device_id][0]);
  Serial.print("Device "); Serial.println(device_name);
  Serial.print("ID "); Serial.println(devices[device_id][1]);
  Serial.print("state: ");
  if (state) {
    Serial.println("ON");
    mySwitch.switchOn(devices[device_id][0], devices[device_id][1]);
  } else {
    Serial.println("OFF");
    mySwitch.switchOff(devices[device_id][0], devices[device_id][1]);
  }
  Serial.println("-----------------------");
  digitalWrite(LED, 1);
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup() {

  // Init serial port and clean garbage
  Serial.begin(SERIAL_BAUDRATE);
  Serial.println();
  Serial.println();

  // Setup
  wifiSetup();
  rfSetup();

  // Fauxmo
  fauxmo.enable(true);

  Serial.printf("[MQTT] Connecting to %s ", mqtt_server);
  client.begin(mqtt_server, net);
  while (!client.connect("Kinderzimmer-Temp")) {
    Serial.print(".");
    delay(100);
  }

  // Fauxmo
  int arraySize = (sizeof(devices) / sizeof(int) / 3);

  for (int i = 0; i < arraySize; i++) {
    fauxmo.addDevice(devices[i][2]);
  }

  Serial.println();
  fauxmo.onMessage(callback);

}
int timeSinceLastRead = 30000;

void loop() {

  fauxmo.handle();

  client.loop();

  if (timeSinceLastRead > 30000) {
    float temp = dht.readTemperature();
    if (!isnan(temp)) {
      char JSONmessageBuffer[100];
      DynamicJsonBuffer jsonBuffer(74);

      JsonObject& root = jsonBuffer.createObject();
      root["Time"] = "0000-00-00T00:00:00";

      JsonObject& SI7021 = root.createNestedObject("SI7021");
      SI7021["Temperature"] = temp;
      SI7021["Humidity"] = dht.readHumidity();
      root["TempUnit"] = "C";

      root.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
      Serial.println("Sending message to MQTT topic..");
      Serial.println(JSONmessageBuffer);

      client.publish("tele/kz/temp/SENSOR", JSONmessageBuffer);
      timeSinceLastRead = 0;
    }
  }
  delay(10);
  timeSinceLastRead += 10;
}


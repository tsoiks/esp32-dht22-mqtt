#include <WiFi.h>
#include <WiFiClient.h>
#include "DHTesp.h"
#include <PubSubClient.h>
#include <secrets.h>
#include "Ticker.h"

#ifndef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP32 ONLY!)
#error Select ESP32 board.
#endif

// Activate MQTT
// defined:     MQTT active - Publish Beacons via MQTT and send tehm over Serial Port
// not defined: No MQTT - Report Beacons only over Serial Port
#define MQTT_ACTIVE
#define MQTT_BUFSIZE  2048 // MQTT max. send size

#define FREQUENCY  3000    // delay between reports

// Globals
DHTesp dht;
WiFiClient myWiFiClient;
PubSubClient mqtt(MQTT_SERVER, MQTT_PORT, myWiFiClient);

 /************************ 
  *  Setup
  ************************ 
  * - init Serial
  * - Init Wifi
  * - Init MQTT
  ************************/
void setup() {
    // int i;
    int timeout;
    
    Serial.begin(115200);
    Serial.println("\n");
    Serial.println("DHT22 over MQTT, not secure");
    Serial.println("===========================");

#ifdef MQTT_ACTIVE
    // Connect to WiFi
    Serial.print("Connecting to WiFi ... ");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_Ssid, WIFI_Key);
    timeout = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        timeout++;
        if (timeout > 60) {
            Serial.println("\r\nWiFi-Connection failed - Rebooting.");
            delay(5000);
            ESP.restart(); 
        }
    }
    Serial.println("connected.");  
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // Connect to MQTT-Server
    Serial.print("Connecting to MQTT-Server ... ");
    if (mqtt.connect(MQTT_NAME, MQTT_USER, MQTT_PASS))  { 
        mqtt.setBufferSize(MQTT_BUFSIZE);
        mqtt.publish(TOPIC_STATUS," ... connected");
        Serial.println("connected.");
    } else {
        Serial.println("\r\nMQTT-Connection failed - Rebooting.");
        delay(5000);
        ESP.restart(); 
    }   
    mqtt.loop();

    dht.setup(17, DHTesp::DHT22); // Connect DHT sensor to GPIO 17

#endif // MQTT_ACTIVE
}

/********************************
 * Main-Loop  
 ********************************
 * - Get Results from DHT22
 * - Poll MQTT-Client
 * - JSON
 *   - Compose 
 *   - Send to Serial
 *   - MQTT Publish 
 *******************************/
void loop() {
  String myJSON;
  char buff[10];

  delay(dht.getMinimumSamplingPeriod());

  String status = dht.getStatusString();
  String temperature = (char*) dtostrf(dht.getTemperature(), 5, 2, buff);
  String humidity = (char*) dtostrf(dht.getHumidity(), 5, 2, buff);

  // Start composing JSON
  myJSON = "{"; 

  myJSON += "\"Status\": \"" + status + "\",";
  myJSON += "\"Temperature\": " + temperature + ",";
  myJSON += "\"Humidity\": " + humidity;
  // Complete JSON String
  myJSON += "}";

#ifdef MQTT_ACTIVE
  mqtt.publish(TOPIC_SCAN, (char*) myJSON.c_str());      
  
  // Reboot if WIFI-Connection lost
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\r\nLost WiFi-Connection - Rebooting.");
      delay(5000);
      ESP.restart(); 
    }
    
  // Try to Reconnect, otherwise Reboot if MQTT-Connection lost
  if (! mqtt.connected()) {  
    Serial.println("\r\nLost MQTT Connection - trying ro reconnect ... ");
    mqtt.connect(MQTT_NAME, MQTT_USER, MQTT_PASS);
    int timeout = 0;
    while (!mqtt.connected()) {
       delay(500);
       Serial.print(".");
       timeout++;
        if  (timeout > 60) {
            Serial.println("failed - Reboot.");
            ESP.restart();
        }
    }
    Serial.println("success.");
    mqtt.publish(TOPIC_STATUS," ... lost connection and reconnected");
  } 
  mqtt.loop();
#endif // MQTT_ACTIVE

  // Send Result to Serial
  Serial.println(myJSON);
  delay(FREQUENCY);

}
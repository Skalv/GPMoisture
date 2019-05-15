/************************/
/*       INCLUDES       */
/************************/
#include <Arduino.h>
#include <ESP8266WiFi.h> // WIFI
#include <WiFiClient.h>  // WIFI
#include <PubSubClient.h>// MQTT
#include <EEPROM.h>      // DB
#include <ArduinoJson.h> // JSON
#include <SettingsManager.h>   // Config file manager
#include <ESP8266TrueRandom.h> // UID generator

/************************/
/*       VARIABLES      */
/************************/
long lastSynchro = 0;
long lastMoistureSynchro = 0;
/************************/
/*       CLASS DEF      */
/************************/
WiFiClient espClient;
PubSubClient client(espClient);
SettingsManager sm;
/************************/
/*       FUNCTIONS      */
/************************/
// Wifi connection
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connexion a ");
  Serial.println(sm.getChar("wifi.ssid", "Not found..."));
 
  WiFi.begin(sm.getChar("wifi.ssid", "GrenPi"), sm.getChar("wifi.password", ""));
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  sm.setString("device.macAddress", WiFi.macAddress());
  sm.writeSettings("/config.json");

  Serial.println("");
  Serial.println("WiFi connected ");
  Serial.print("=> Addresse IP : ");
  Serial.println(WiFi.localIP());
  Serial.print("=> Addresse MAC : ");
  Serial.println(WiFi.macAddress());
  Serial.println("");
}
// MQTT connection
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(sm.getChar("device.uid", ""));
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
// MQTT Callback
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("");
  Serial.println("=== NEW MESSAGE ===");
  Serial.println("Topic: " + String(topic));
  Serial.println("Length: " + String(length,DEC));

  StaticJsonDocument<256> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, payload, length);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  serializeJsonPretty(jsonDoc, Serial);

  messageHandler(jsonDoc.as<JsonObject>());
}
// MQTT ROUTER
void messageHandler(JsonObject doc) {
  String action = doc["action"];

  if (action == "boardSynchro") {
    String state = doc["state"];
    String uid = doc["uid"];
    if (state == "ok") {
      if (sm.getString("device.uid") != uid) {
        sm.setString("device.uid", uid);
      }
      sm.setBool("device.synchronised", true);
      sm.writeSettings("/config.json");
      Serial.println("Synchronisé !");
    }
  }

  if (action == "command") {
    String value = doc["value"];

    if (value == "state") {
      readMoisture();
    }
  }
}
// Synchro Manager
void handleSynchro() {
  long now = millis();
  if (now - lastSynchro > 1000 * 30) {
    lastSynchro = now;
    Serial.println("Envoi d'un message de synchro");
    
    char buffer[256];
    serializeJson(sm.getJsonObject("device"), buffer);
    
    client.publish(sm.getChar("mqtt.topics.synchro","helloGP"), buffer, false);
  }
}

void readMoisture() {
  long now = millis();
  if (now - lastMoistureSynchro > 1000 * 600) {
    lastMoistureSynchro = now;

    int value = analogRead(sm.getInt("device.pin"));
    Serial.print("Moisture level : ");
    Serial.println(value);

    const size_t capacity = JSON_OBJECT_SIZE(2);
    DynamicJsonDocument doc(capacity);
    doc["action"] = "response";
    doc["state"] = value;

    char buffer[512];
    serializeJson(doc, buffer);
    client.publish(sm.getChar("device.uid", ""), buffer, true);
  }
}

void setup() {
  Serial.begin(9600);
  
  // Settings manager
  sm.readSettings("/config.json"); //Loading json from file config.json
  
  // Connect Wifi and MQTT
  setup_wifi();
  client.setServer(sm.getChar("mqtt.server", ""), 1883);
  client.setCallback(mqtt_callback);

  // Manage UID
  String uid = sm.getString("device.uid");
  if (uid.length() == 0) {
    byte uuidNumber[16];
    ESP8266TrueRandom.uuid(uuidNumber);
    String uuid = ESP8266TrueRandom.uuidToString(uuidNumber);
    int res = sm.setString("device.uid", uuid);
    sm.writeSettings("/config.json");
  }

  // Setup GPIOS
  pinMode(sm.getUInt("device.pin"), INPUT);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (!sm.getBool("device.synchronised")) {
    handleSynchro();
  }
  readMoisture();
}
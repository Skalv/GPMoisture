/************************/
/*       INCLUDES       */
/************************/
#include <Arduino.h>
#include <ESP8266WiFi.h> // WIFI
#include <WiFiClient.h>  // WIFI
#include <PubSubClient.h>// MQTT
/************************/
/*       CONSTS         */
/************************/
// WIFI
#define wifi_ssid "GreenPi"
#define wifi_password "greenpimdp123"
// MQTT 
#define mqtt_server "10.3.141.1"
#define synchro_topic "helloGP"
// GPIOS
#define periphPin D1
/************************/
/*       VARIABLES      */
/************************/
// MQTT
String TYPE = "_lamp";
String MQTTTopic;
boolean isSynchro = false;
long lastSynchro = 0;
// GPIOS
boolean periphState = false;
/************************/
/*       CLASS DEF      */
/************************/
WiFiClient espClient;
PubSubClient client(espClient);
/************************/
/*       FUNCTIONS      */
/************************/
// Wifi connection
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connexion a ");
  Serial.println(wifi_ssid);
 
  WiFi.begin(wifi_ssid, wifi_password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected ");
  Serial.print("=> Addresse IP : ");
  Serial.println(WiFi.localIP());
  Serial.print("=> Addresse MAC : ");
  Serial.println(WiFi.macAddress());
  Serial.println("");

  MQTTTopic = WiFi.macAddress() + TYPE;
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
      //client.subscribe(synchro_topic);
      client.subscribe(MQTTTopic.c_str());
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
  int i = 0;
  char s[length];
  
  Serial.println("");
  Serial.println("=== NEW MESSAGE ===");
  Serial.println("Topic: " + String(topic));
  Serial.println("Length: " + String(length,DEC));
  
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    s[i] = payload[i];
  }
  s[i] = '\0';

  Serial.print("payload: ");
  Serial.println(s);

  messageHandler(s);
}
// MQTT ROUTER
void messageHandler(String message) {
  if (message == "synchronised") {
    isSynchro = true;
    Serial.println("Synchronisé !");
  } else if(message == "ON") {
    digitalWrite(periphPin, HIGH);
    periphState = true;
  } else if (message == "OFF") {
    digitalWrite(periphPin, LOW);
    periphState = false;
  } else if (message == "toggle") {
    periphState = !periphState;
    digitalWrite(periphPin, periphState ? HIGH : LOW);
  } else if (message == "state") {
    client.publish(MQTTTopic.c_str(), String(periphState).c_str(), true);
  }
}
// Synchro Manager
void handleSynchro() {
  long now = millis();
  if (now - lastSynchro > 1000 * 20) {
    lastSynchro = now;
    Serial.println("Envoi d'un message de synchro");

    String SynchroMsg = "{\"topic\":\"" + MQTTTopic + "\",\"type\":\"lamp\"}";

    client.publish(synchro_topic, SynchroMsg.c_str(), true);
  }
}

void setup() {
  Serial.begin(9600);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqtt_callback);

  // Setup GPIOS
  pinMode(periphPin, OUTPUT);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (!isSynchro) {
    handleSynchro();
  }
}
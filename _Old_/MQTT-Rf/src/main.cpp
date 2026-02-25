#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <RCSwitch.h>

const char* ssid = "Julianno-WiFi";
const char* password = "JULIANf1";
const char* mqtt_server = "192.168.1.200";

WiFiClient espClient;
PubSubClient client(espClient);
RCSwitch mySwitch = RCSwitch();

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(message);

  if (String(topic) == "pantalla/action") {
    if (message == "ON") {
      Serial.println("Bajando pantalla...");
      mySwitch.send(704612, 24);  // Código para bajar pantalla
      client.publish("pantalla/state", "on", true);  // Retener estado
    } else if (message == "OFF") {
      Serial.println("Subiendo pantalla...");
      mySwitch.send(704610, 24);  // Código para subir pantalla
      client.publish("pantalla/state", "off", true);  // Retener estado
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      client.subscribe("pantalla/action");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  mySwitch.enableTransmit(5);  // Usa el pin GPIO5
  mySwitch.setRepeatTransmit(10);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
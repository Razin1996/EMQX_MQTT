#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi
const char* ssid = "Onutiative";
const char* password = "P@$50fW1f1";

// MQTT Broker
const char *mqtt_broker = "103.98.206.92";
const char *topic1 = "machine_id";
//const char *topic2 = "esp8266/pubsub";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

//for future customer userID
//mqtt_customer_usename = "";
//mqtt_customer_password = "";

WiFiClient espClient;
PubSubClient client(espClient);

 char msgC;
 String msgS;
 String command;
 String message;
void setup() {
 // Set software serial baud to 115200;
 Serial.begin(115200);
 // connecting to a WiFi network
 WiFi.begin(ssid, password);
 while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.println("Connecting to WiFi..");
 }
 Serial.println("Connected to the WiFi network");
 //connecting to a mqtt broker
 client.setServer(mqtt_broker, mqtt_port);
 client.setCallback(callback);
 while (!client.connected()) {
//     String client_id = "esp8266-client-";
//     client_id += String(WiFi.macAddress());
     Serial.println("Connecting to public emqx mqtt broker.....");
     if (client.connect("esp8266-vendy2")) {
         Serial.println("Public emqx mqtt broker connected");
     } else {
         Serial.print("failed with state ");
         Serial.print(client.state());
         delay(2000);
     }
 }
 // publish and subscribe
  client.subscribe(topic1);

//    client.publish(topic2, "hello user");

}

void callback(char *topic, byte *payload, unsigned int length) {
 Serial.print("Message arrived in topic: ");
 Serial.println(topic);
 Serial.print("Message:");
 for (int i = 0; i < length; i++) {
     msgC = (char) payload[i];
     String temp = String(msgC);
     msgS = msgS + temp;
     
 }
  messageDecoder(msgS);
}

void messageDecoder(String response){
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, response);
  JsonArray repos = doc["message"];
  for (JsonObject repo : repos) {
    Serial.println("");
    Serial.print("Dispancer: ");
    Serial.println(repo["dispancer_id"].as<int>());
  }
}

void loop() {
 client.loop();
}

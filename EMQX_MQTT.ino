#include <Arduino.h>
#include <String.h>
#include <EEPROM.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

// WiFi
const char* WIFI_SSID = "BONYASHOVA-3";
const char* WIFI_PASSWORD = "9294959699";
//const char* WIFI_SSID = "ASUS";
//const char* WIFI_PASSWORD = "asus2222";
#define busy 16                                                   // connect to D0 pin of NodeMCU from green blue
#define connection 15                                             // connect to D1 pin of NodeMCU from green led
#define motor_positive 4                                          // connect to D2 pin of NodeMCU
#define disconnection 0                                           // connect to D3 pin of NodeMCU from red led
#define TX1 2                                                     // NOT connected to D4 pin of NodeMCU
#define motor_negative 14                                         // connect to D5 pin of NodeMCU
#define rotation_input 12                                         // connect to D6 pin of NodeMCU
#define RDM6300_RX_PIN 13                                         // connect to D7 pin of NodeMCU force hardware uart
#define limit 5                                                   // NOT connected to D8 of NodeMCU (reserved for ouput only)
#define buzzer 1 
//#define rst 14                                                    // connect to D5 pin of NodeMCU

// MQTT Broker
const char *mqtt_broker = "103.98.206.92";
const char *topic1 = "machine_id";
const char *topic2 = "pubsub";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

//for future customer userID
//mqtt_customer_usename = "";
//mqtt_customer_password = "";

WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WiFiMulti WifiMulti;

int B=0,fb=0;
int is_disable = 0;
int is_door_lock = 1;
boolean default_status = 0;
boolean database_connected = 0;
boolean resetToDefault = 0;
boolean end_of_line = 0;
volatile boolean rotation_count = 0;
char p[16];
char s[16];

 
void setup() {
 // Set software serial baud to 115200;
  Serial1.begin(115200);

  int count = 0;
  pinMode(connection, OUTPUT);
  pinMode(disconnection, OUTPUT);
  pinMode(busy, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(motor_positive, OUTPUT);
  pinMode(motor_negative, OUTPUT);
  pinMode(rotation_input, INPUT_PULLUP); 
  pinMode(limit, INPUT_PULLUP);
//  pinMode(rst, INPUT_PULLUP);
  digitalWrite(busy, HIGH);
  digitalWrite(connection, LOW);
  digitalWrite(disconnection,LOW);
  digitalWrite(buzzer, HIGH);
  delay(100);
  digitalWrite(buzzer, LOW);
  digitalWrite(motor_positive, LOW);
  digitalWrite(motor_negative, LOW);

  Serial1.println("Program Started");
 
 EEPROM.begin(512);
// for (int i = 0; i < 512; i++) {
//    EEPROM.write(i, '0');
//  }
  Serial1.print("EEPROM: ");
  Serial1.println(EEPROM.read(100));
 if(EEPROM.read(100) == 55)
    user_ap();
  else
    default_ap(); 
 //connecting to a mqtt broker
 client.setServer(mqtt_broker, mqtt_port);
 client.setCallback(callback);
 while (!client.connected()) {
//     String client_id = "esp8266-client-";
//     client_id += String(WiFi.macAddress());
     Serial1.println("Connecting to public emqx mqtt broker.....");
     if (client.connect("esp8266-vendy1")) {
         Serial1.println("Public emqx mqtt broker connected");
     } else {
         Serial1.print("failed with state ");
         Serial1.print(client.state());
         delay(2000);
     }
 }
 // publish and subscribe
  client.subscribe(topic1);
  client.subscribe(topic2);

}

void callback(char *topic, byte *payload, unsigned int length) {
 Serial1.print("Message arrived in topic: ");
 Serial1.println(topic);
 Serial1.print("Message: ");
 char msgC;
 String msgS;
 for (int i = 0; i < length; i++) {
     msgC = (char) payload[i];
     String temp = String(msgC);
     msgS = msgS + temp;
 }
   Serial1.println(msgS);
   String command = commandDecoder(msgS);
   if(command == "DISPENSE"){
      Serial1.println("dispensing");
      messageDecoderDispense(msgS);
    }
    else if(command == "SETTINGS"){
      Serial1.println("settings changed");
      settings_change(msgS);
      }
    else if(command == "STATUS_QUERY"){
      send_status();
      }
    else if(command == "REFILL"){
      
      }
    else if(command == "DISCARD"){
      
      }
    //client.connect("esp8266-vendy1");
}

void loop() {
 if (!client.connected()) {
    client.connect("esp8266-vendy1");
    client.subscribe(topic1);
  }
 client.loop();

}

void messageDecoderDispense(String response){
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, response);
  JsonArray repos = doc["message"];
  int dis_quantity;
  for (JsonObject repo : repos) {
    Serial1.println("");
    Serial1.print("Dispenser: ");
    Serial1.println(repo["dispenser_id"].as<int>());
    dis_quantity = repo["quantity"].as<int>();
    //quantity = dis_quantity.toInt();
    Serial1.println(dis_quantity);
    dispense(dis_quantity);
  }
  
}

String commandDecoder(String response){
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, response);
  auto command = doc["command"].as<char*>();
  //Serial.println(command);
  return command;
}

//void readRFID(){
//  delay(10);
//  if (rdm6300.update()){
//    digitalWrite(busy, HIGH);
//    digitalWrite(connection, LOW);
//    digitalWrite(disconnection, HIGH);
//    digitalWrite(buzzer,HIGH);
//    delay(100);
//    digitalWrite(buzzer,LOW);
//    String rfId= String(rdm6300.get_tag_id(),HEX);
//    Serial1.println(rfId);
//    A++;
//    rdm6300.end();
//    delay(100);
//    rdm6300.begin(RDM6300_RX_PIN);
//    rfid_card_status = 0;
//    ledIndicator();
//  }
//}

void dispense(int quantity){
  if(is_disable == 0 && is_door_lock == 1){
    Serial1.println(quantity);
    while(quantity > 0){
//      Serial1.println("Dispensing Continue");
//
//        Serial1.println("Dispensing = "+ quantity);
//        quantity--;
//        delay(200);
//        yield();
       
      if(digitalRead(rotation_input) == 0 ){
        while(digitalRead(rotation_input) == 0){
          digitalWrite(motor_positive, HIGH);
          digitalWrite(motor_negative, LOW);
          delay(200);
          if(!digitalRead(limit))
            end_of_line = 1;        
          yield();
        }
      }
      if(digitalRead(rotation_input)){
        while(digitalRead(rotation_input)){
          digitalWrite(motor_positive, HIGH);
          digitalWrite(motor_negative, LOW);
          delay(200);
          if(!digitalRead(limit))
            end_of_line = 1;
          yield();
        }
        while(digitalRead(rotation_input) == 0){
          digitalWrite(motor_positive, HIGH);
          digitalWrite(motor_negative, LOW);
          delay(200);
          if(!digitalRead(limit))
            end_of_line = 1;
          yield(); 
        }
        digitalWrite(motor_positive, LOW); 
        digitalWrite(motor_negative, LOW);
        digitalWrite(busy, HIGH);
        digitalWrite(connection, HIGH);
        Serial1.println("Dispensing = "+ quantity);
        quantity--;
  //      do{
  //        Serial1.println(A);
  //        JsonObject& object = jsonBuffer.createObject();
  //        object["A"] = A;
  //        JsonObject& feedback = putFirebaseData("product.json", object);
  //        fb = feedback.get<int>("A");
  //        jsonBuffer.clear();
  //        yield();
  //        ledIndicator();
  //        digitalWrite(busy, HIGH);
  //      }while(A != fb);
      }
      if(end_of_line){
        if(digitalRead(limit) == 0){
          digitalWrite(motor_positive, LOW);
          digitalWrite(motor_negative, HIGH);
          delay(10000);
        }
        if(digitalRead(limit)){
          while(digitalRead(limit)){
            digitalWrite(motor_positive, LOW);
            digitalWrite(motor_negative, HIGH);
            delay(200);
            yield();
          }
          digitalWrite(motor_positive, LOW);
          digitalWrite(motor_negative, LOW);
          delay(500);
          while(!digitalRead(limit)){
            digitalWrite(motor_positive, HIGH);
            digitalWrite(motor_negative, LOW);
            delay(200);
            yield();            
          }
          digitalWrite(motor_positive, LOW);
          digitalWrite(motor_negative, LOW);
        }
        end_of_line = 0;
      }
      yield();
    }
  }
  digitalWrite(connection, LOW);
  digitalWrite(motor_positive, LOW);
  digitalWrite(motor_negative, LOW);
}

void settings_change(String response){
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, response);
//     auto s = doc["message"]["ssid"].as<char*>();
//     auto p = doc["message"]["password"].as<char*>();
     String u_ssid=String(doc["message"]["ssid"].as<char*>());
     String u_pass=String(doc["message"]["password"].as<char*>());
    is_disable = doc["message"]["is_disable"].as<int>();
    is_door_lock = doc["message"]["is_door_lock"].as<int>();
    Serial1.println(u_ssid);
    Serial1.println(u_pass);
    writeString(0, u_ssid);
    writeString(16,u_pass);
    delay(10);
    Serial1.println(is_disable);
    Serial1.println(is_door_lock);
//    EEPROM.put(0,s);
//    EEPROM.put(16,p);
    EEPROM.write(100, 55);
    EEPROM.commit();
    delay(10);
//    if(digitalRead(rst) == 0){
//    int x = 0;
//    if(default_status == 1){
//      user_ap();
//    }
//    else{
//      while(digitalRead(rst) ==0){
//        delay(100);
//        x++;
//        if(x>30){
//          default_ap();
//          break;
//        }
//        yield();
//      }
//    }
//  }
    user_ap();
  }

void send_status(){
  
  }

void user_ap(){  
//    EEPROM.get(0, s);
    String ssid=read_String(0);
    String pass=read_String(16);
    Serial1.print(ssid);
    Serial1.print("\t");
//    EEPROM.get(16, p);
    Serial1.println(pass);
    delay(10);              
    WiFi.begin(ssid, pass);
    Serial1.print("Connecting to user ");
    Serial1.print(ssid);
    while (WiFi.status() != WL_CONNECTED) {
      Serial1.print(".");
//      if(digitalRead(rst) == 0){
//        break;
//      }
      yield();
      delay(1000);
    }
    Serial1.println();
    Serial1.print("Connected to ");
    Serial1.println(ssid);
    Serial1.print("IP Address is : ");
    Serial1.println(WiFi.localIP());
    default_status = 0;
}

void default_ap(){
  delay(10);  
   // connecting to a WiFi network           
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial1.print("Connecting to defult ");
  Serial1.print(WIFI_SSID);
  int wifi_check = 0;
  while(WiFi.status() != WL_CONNECTED) {
    wifi_check++;
    if(wifi_check == 20){
      default_status = 0;
      Serial1.println();
      Serial1.println("Connection Failed");
      return;
    }
    yield();
    Serial1.print(".");
    delay(1000);
  }
  Serial1.println();
  Serial1.print("Connected to ");
  Serial1.println(WIFI_SSID);
  Serial1.print("IP Address is : ");
  Serial1.println(WiFi.localIP());
  default_status = 1;
  digitalWrite(busy, HIGH);
  digitalWrite(connection, LOW);
  digitalWrite(disconnection, LOW);
}

void ledIndicator(){
  if(database_connected){
    if(default_status){
      digitalWrite(busy, HIGH);
      digitalWrite(connection, LOW);
      digitalWrite(disconnection, LOW);
    }
    else{
      digitalWrite(busy, LOW);
      digitalWrite(connection, HIGH);
      digitalWrite(disconnection, LOW);      
    }
  }
  else{
    digitalWrite(busy, LOW);
    digitalWrite(connection, LOW);
    digitalWrite(disconnection, HIGH);      
  }
}

//EEPROM
void writeString(char index,String data)
{
  int _size = data.length();
  int i;
  for(i=0;i<_size;i++)
  {
    EEPROM.write(index+i,data[i]);
  }
  EEPROM.write(index+_size,'\0');   //Add termination null character for String Data
  EEPROM.commit();
}


String read_String(char index)
{
  int i;
  char data[100]; //Max 100 Bytes
  int len=0;
  unsigned char k;
  k=EEPROM.read(index);
  while(k != '\0' && len<500)   //Read until null character
  {    
    k=EEPROM.read(index+len);
    data[len]=k;
    len++;
  }
  data[len]='\0';
  return String(data);
}

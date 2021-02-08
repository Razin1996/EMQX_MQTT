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
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// WiFi
//String WIFI_SSID;
//String WIFI_PASSWORD;
//const char* WIFI_SSID = "Onutiative";
//const char* WIFI_PASSWORD = "P@$50fW1f1";

//#define busy 16                                                   // connect to D0 pin of NodeMCU from green blue
#define connection 15                                             // connect to D1 pin of NodeMCU from green led
#define motor_positive 4                                          // connect to D2 pin of NodeMCU
#define disconnection 0                                           // connect to D3 pin of NodeMCU from red led
#define TX1 2                                                     // NOT connected to D4 pin of NodeMCU
#define motor_negative 14                                         // connect to D5 pin of NodeMCU
#define rotation_input 12                                         // connect to D6 pin of NodeMCU
#define RDM6300_RX_PIN 13                                         // connect to D7 pin of NodeMCU force hardware uart
#define limit 5                                                   // NOT connected to D8 of NodeMCU (reserved for ouput only)
#define buzzer 1 
#define rst 16                                                    // connect to D0 pin of NodeMCU

// MQTT Broker
const char *mqtt_broker = "103.98.206.92";
const char *topic;
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
boolean default_status = 0;
boolean database_connected = 0;
boolean resetToDefault = 0;
boolean end_of_line = 0;
volatile boolean rotation_count = 0;

const char* ap_ssid = "NodeMCU";
const char* ap_pass = "123456789";
IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
AsyncWebServer server(80);

const char* login_PARAM_INPUT_1 = "User_ID";
const char* login_PARAM_INPUT_2 = "Password";
const char* value_PARAM_INPUT_1 = "Device_ID";
const char* value_PARAM_INPUT_2 = "Wifi_SSID";
const char* value_PARAM_INPUT_3 = "Wifi_Pass";
String ap_userID;
String ap_userPass;
String ap_deviceID;
String ap_wifi_SSID;
String ap_wifi_Pass;
int form_updated = 0;

// HTML web page to handle 3 input fields (input1, input2, input3)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/get">
    Device ID: <input type="text" name="Device_ID">
    <br>
    <br>
    Wifi SSID: <input type="text" name="Wifi_SSID">
    <br>
    <br>
    Wifi Password: <input type="text" name="Wifi_Pass">
    <br>
    <input type="submit" value="Submit">
  </form><br>
  </body></html>)rawliteral";

const char login_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Login Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/get">
    User ID: <input type="text" name="User_ID">
    <br>
    <br>
    Password: <input type="text" name="Password">
    <br>
    <input type="submit" value="Submit">
  </form><br>
  </body></html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}
 
void setup() {
 // Set software serial baud to 115200;
  Serial1.begin(115200);
  EEPROM.begin(512);
//  for(int i=0;i<=512;i++){
//    EEPROM.write(i,0);
//    EEPROM.commit();
//    delay(10);
//  }
  writeString(35, "admin");
  writeString(45, "admin");
  
  int count = 0;
  pinMode(connection, OUTPUT);
  pinMode(disconnection, OUTPUT);
  //pinMode(busy, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(motor_positive, OUTPUT);
  pinMode(motor_negative, OUTPUT);
  pinMode(rotation_input, INPUT_PULLUP); 
  pinMode(limit, INPUT_PULLUP);
  pinMode(rst, INPUT_PULLUP);
  //digitalWrite(busy, HIGH);
  digitalWrite(connection, LOW);
  digitalWrite(disconnection,LOW);
  digitalWrite(buzzer, HIGH);
  delay(100);
  digitalWrite(buzzer, LOW);
  digitalWrite(motor_positive, LOW);
  digitalWrite(motor_negative, LOW);

  Serial1.println("Program Started");
  if(digitalRead(rst) == 0){
      EEPROM.write(100,0);
      EEPROM.commit();
      delay(10);
    }

    Serial1.println("AP start");
    WiFi.softAP(ap_ssid, ap_pass);
    WiFi.softAPConfig(local_ip, gateway, subnet);
  //server.on("/", apmode_login);
              // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", login_html);});

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    //GET login form data point
    if (request->hasParam(login_PARAM_INPUT_1)&& request->hasParam(login_PARAM_INPUT_2)) {
      ap_userID = request->getParam(login_PARAM_INPUT_1)->value();
      ap_userPass = request->getParam(login_PARAM_INPUT_2)->value();
    }
    Serial1.println(ap_userID);
    Serial1.println(ap_userPass);
   if(ap_userID == read_String(35) && ap_userPass == read_String(45)){
      request->send_P(200, "text/html", index_html);
    }else{
      request->send_P(200, "text/html", "Username or Password Error!!");
    }
    
    //2nd page
    if (request->hasParam(value_PARAM_INPUT_1)&&request->hasParam(value_PARAM_INPUT_2)&&request->hasParam(value_PARAM_INPUT_3)) {
      ap_deviceID = request->getParam(value_PARAM_INPUT_1)->value();
      ap_wifi_SSID = request->getParam(value_PARAM_INPUT_2)->value();
      ap_wifi_Pass = request->getParam(value_PARAM_INPUT_3)->value();
    }
    form_updated = 1;
    request->send(200, "text/html", "Values have been set successfully.");
    Serial1.println(ap_deviceID);
    Serial1.println(ap_wifi_SSID);
    Serial1.println(ap_wifi_Pass);
    if(ap_deviceID!=""){
      writeString(55, ap_deviceID);
    }
    if(ap_wifi_SSID!="" && ap_wifi_Pass!=""){
      writeString(65, ap_wifi_SSID);
      writeString(75, ap_wifi_Pass);
      EEPROM.write(100,55);
      EEPROM.commit();
      delay(10);
      user_ap();
      configMQTT();
    }
  });
  server.onNotFound(notFound);
  server.begin();
 
  Serial1.print("EEPROM: ");
  Serial1.println(EEPROM.read(100));
   if(EEPROM.read(100) == 55){
      user_ap();
      configMQTT();
  }else{
    Serial1.println("Reset mode");
  }

}

void configMQTT(){
  //connecting to a mqtt broker
  Serial1.println(read_String(55));
  topic = read_String(55).c_str();
  while(form_updated == 1){
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    Serial1.println("Connecting to public emqx mqtt broker.....");
    if (client.connect("esp8266-vendy4")) {
      Serial1.println("Public emqx mqtt broker connected");
     } else {
       Serial1.print("failed with state ");
       Serial1.print(client.state());
       delay(2000);
     }
  }
  // publish and subscribe
  Serial1.print("Topic: ");
  Serial1.println(topic);
  client.subscribe(topic);
  client.subscribe(topic2);
 }
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
}

void loop() {
   if(EEPROM.read(100) == 55){
    if (!client.connected()) {
      client.connect("esp8266-vendy4");
      client.subscribe(topic);
      client.subscribe(topic2);
    }
    client.loop();
   }
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
  if(EEPROM.read(33) == 0 && EEPROM.read(34) == 1){
    while(quantity > 0){       
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
//        digitalWrite(busy, HIGH);
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
//    is_disable = doc["message"]["is_disable"].as<int>();
//    is_door_lock = doc["message"]["is_door_lock"].as<int>();
    String is_disable = String(doc["message"]["is_disable"].as<char*>());
    String is_door_lock = String(doc["message"]["is_door_lock"].as<char*>());
    Serial1.println(u_ssid);
    Serial1.println(u_pass);
    writeString(0, u_ssid);
    writeString(16, u_pass);
    delay(10);
    Serial1.println(is_disable);
    Serial1.println(is_door_lock);
    writeString(33, is_disable);
    writeString(34, is_door_lock);
    EEPROM.write(100, 55);
    EEPROM.commit();
    delay(10);
    user_ap();
  }

void send_status(){
  
  }

void user_ap(){  
    String ssid=read_String(0);
    String pass=read_String(16);
    Serial1.print(ssid);
    Serial1.print("\t");
    Serial1.println(pass);
    delay(10);              
    WiFi.begin(ssid, pass);
    Serial1.print("Connecting to user ");
    Serial1.print(ssid);
    while (WiFi.status() != WL_CONNECTED) {
    Serial1.print(".");
//      if(digitalRead(rst) == 0){
//        default_ap();
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
              // Send web page with input fields to client
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
      });
}
//
//void default_ap(){
//  delay(10);  
//  String WIFI_SSID = read_String(65);
//  String WIFI_PASSWORD = read_String(75);
//  while(form_updated == 1){
//   // connecting to a WiFi network           
//  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
//  Serial1.print("Connecting to default ");
//  Serial1.print(WIFI_SSID);
//  int wifi_check = 0;
//  while(WiFi.status() != WL_CONNECTED) {
//    wifi_check++;
//    if(wifi_check == 20){
//      Serial1.println("Connection Failed");
//      return;
//    }
//    yield();
//    Serial1.print(".");
//    delay(1000);
//  }
//  Serial1.println();
//  Serial1.print("Connected to ");
//  Serial1.println(WIFI_SSID);
//  Serial1.print("IP Address is : ");
//  Serial1.println(WiFi.localIP());
//  //digitalWrite(busy, HIGH);
//  digitalWrite(connection, LOW);
//  digitalWrite(disconnection, LOW);
//  }     
//}



//void ledIndicator(){
//  if(database_connected){
//    if(default_status){
//      digitalWrite(busy, HIGH);
//      digitalWrite(connection, LOW);
//      digitalWrite(disconnection, LOW);
//    }
//    else{
//      digitalWrite(busy, LOW);
//      digitalWrite(connection, HIGH);
//      digitalWrite(disconnection, LOW);      
//    }
//  }
//  else{
//    digitalWrite(busy, LOW);
//    digitalWrite(connection, LOW);
//    digitalWrite(disconnection, HIGH);      
//  }
//}

//AP Mode
//void apmode_value(){
//        // Send web page with input fields to client
//      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
//        request->send_P(200, "text/html", index_html);
//      });
//        // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
//  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
//    String inputMessage;
//    String inputParam;
//    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
//    if (request->hasParam(value_PARAM_INPUT_1)&&request->hasParam(value_PARAM_INPUT_2)&&request->hasParam(value_PARAM_INPUT_3)) {
//      ap_deviceID = request->getParam(value_PARAM_INPUT_1)->value();
//      ap_wifi_SSID = request->getParam(value_PARAM_INPUT_1)->value();
//      ap_wifi_Pass = request->getParam(value_PARAM_INPUT_1)->value();
//    }
//    else {
////      inputMessage = "No message sent";
////      inputParam = "none";
//      Serial1.println("No message sent");
//    }
//    writeString(55, ap_deviceID);
//    writeString(65, ap_wifi_SSID);
//    writeString(75, ap_wifi_Pass);
//    Serial1.println(ap_deviceID);
//    Serial1.println(ap_wifi_SSID);
//    Serial1.println(ap_wifi_Pass);
//    request->send(200, "text/html", "Values have been set successfully.");
//  });
//  form_updated = 1;
//  server.begin();
// }

// void apmode_login(){
//            // Send web page with input fields to client
//      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
//        request->send_P(200, "text/html", login_html);
//      });
//
//        // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
//  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
//      String inputMessage;
//    String inputParam;
//    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
//    if (request->hasParam(PARAM_INPUT_1)) {
//      ap_userID = request->getParam(PARAM_INPUT_1)->value();
//      inputParam = PARAM_INPUT_1;
//    }
////    // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
////    else if (request->hasParam(PARAM_INPUT_2)) {
////      inputMessage = request->getParam(PARAM_INPUT_2)->value();
////      inputParam = PARAM_INPUT_2;
////    }
//    else {
//      inputMessage = "No message sent";
//      inputParam = "none";
//    }
//    Serial1.println(ap_userID);
//    request->send(200, "text/html", "HTTP GET request sent to your ESP on input field (" 
//                                     + inputParam + ") with value: " + ap_userID +
//                                     "<br><a href=\"/\">Return to Home Page</a>");
//  });
//  }

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

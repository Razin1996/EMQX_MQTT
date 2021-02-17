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
#include <ESP8266HTTPClient.h>
#include <rdm6300.h>
#include <SoftwareSerial.h>
#include <DFMiniMp3.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//important comments
/*
 * machineID=topic
 * 
 */

// WiFi
//String WIFI_SSID;
//String WIFI_PASSWORD;
//const char* WIFI_SSID = "Onutiative";
//const char* WIFI_PASSWORD = "P@$50fW1f1";

// RFID
#define RDM6300_RX_PIN 13                                         // connect to D7 pin of NodeMCU force hardware uart

// MQTT Broker
const char *mqtt_broker = "103.98.206.92";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WiFiMulti WifiMulti;
Rdm6300 rdm6300;

//AP Mode and Server Settings
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

//Mp3 Operation
// implement a notification class,
// its member methods will get called 
//
class Mp3Notify
{
public:
  static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action)
  {
    if (source & DfMp3_PlaySources_Sd) 
    {
        Serial1.print("SD Card, ");
    }
    if (source & DfMp3_PlaySources_Usb) 
    {
        Serial1.print("USB Disk, ");
    }
    if (source & DfMp3_PlaySources_Flash) 
    {
        Serial1.print("Flash, ");
    }
    Serial1.println(action);
  }
  static void OnError(uint16_t errorCode)
  {
    // see DfMp3_Error for code meaning
    Serial1.println();
    Serial1.print("Com Error ");
    Serial1.println(errorCode);
  }
  static void OnPlayFinished(DfMp3_PlaySources source, uint16_t track)
  {
    Serial1.print("Play finished for #");
    Serial1.println(track);  
  }
  static void OnPlaySourceOnline(DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "online");
  }
  static void OnPlaySourceInserted(DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "inserted");
  }
  static void OnPlaySourceRemoved(DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "removed");
  }
};

//OLED Display
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);
#define LOGO_HEIGHT   30
#define LOGO_WIDTH    30

//Vendy Logo
static const unsigned char PROGMEM logo_bmp[] =
{
0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc, 
  0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0x8f, 0xfc, 0xff, 0xff, 0x07, 0xfc, 0xff, 0xfc, 0x03, 0xfc, 
  0x80, 0x18, 0x01, 0xfc, 0x87, 0xf0, 0x01, 0xfc, 0x87, 0xe0, 0x00, 0xfc, 0x87, 0xf8, 0x00, 0xfc, 
  0x87, 0xfe, 0x00, 0xfc, 0x87, 0x80, 0x00, 0x3c, 0x87, 0x00, 0x00, 0x3c, 0x87, 0x00, 0x00, 0xfc, 
  0x87, 0x00, 0x01, 0x8c, 0x87, 0x80, 0x07, 0x8c, 0x87, 0xc3, 0xef, 0x8c, 0x87, 0xff, 0xff, 0x8c, 
  0x87, 0xff, 0xff, 0x8c, 0x87, 0xff, 0xff, 0x8c, 0x87, 0xff, 0xff, 0x8c, 0x80, 0x00, 0x00, 0x0c, 
  0xc0, 0x00, 0x00, 0x1c, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc, 
  0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc,
};

//////////////////////////////////////////////////////////////

//Signal Output
int door=3;
int red = 4;
int green = 5;
int blue = 6;
int buzzer = 7;

// Signal input
int res = 0;
int in1 = 1;
int in2 = 2;

int latchPin = 12;
int clockPin = 16;
int dataPin = 14;
byte regByte=0;

void setup() {
 // Set software Serial1 baud to 115200;
  Serial1.begin(115200);
  EEPROM.begin(512);
//  for(int i=0;i<=512;i++){
//    EEPROM.write(i,0);
//    EEPROM.commit();
//    delay(10);
//  }
  writeString(35, "admin");
  writeString(45, "admin");

  Serial1.println("Program Started");
  if(!1){
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
   request->send(200, "text/html", "Values have been set successfully.");
   Serial1.println(ap_deviceID);
   Serial1.println(ap_wifi_SSID);
   Serial1.println(ap_wifi_Pass);
   if(ap_deviceID!=""){
      writeString(55, ap_deviceID);
    }
   if(ap_wifi_SSID!="" && ap_wifi_Pass!=""){
     writeString(0, ap_wifi_SSID);
     writeString(16, ap_wifi_Pass);
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
  rdm6300.begin(RDM6300_RX_PIN);
//  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
//  display.clearDisplay();
//  display.display();
  Serial1.println("RFID Started");
  configMp3();
  
  //OLED Setup
  Serial1.println("OLED FeatherWing test");
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  Serial1.println("OLED begun");
  display.display();
  delay(1000);
  display.clearDisplay();
  display.display();
  Serial1.println("IO test");
  
  testdrawbitmap();

  // Shift Register
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);  
  pinMode(clockPin, OUTPUT);
}

void configMQTT(){
  //connecting to a mqtt broker
  String romTopic=read_String(55);
  String romClient=read_String(55);
  Serial1.println(romTopic);
  const char *topic = romTopic.c_str();
  const char *clientID = romClient.c_str();
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    Serial1.println("Connecting to public emqx mqtt broker.....");
    if (client.connect(clientID)) {
      Serial1.println("Public emqx mqtt broker connected");
     } else {
       Serial1.print("failed with state ");
       Serial1.print(client.state());
       delay(2000);
     }
  }
  // publish and subscribe
  Serial1.print("Topic in config: ");
  Serial1.println(topic);
  client.subscribe(topic);
//  digitalWrite(red,HIGH);
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

// DFPlayer 
SoftwareSerial secondarySerial(12, 15); // RX, TX
DFMiniMp3<SoftwareSerial, Mp3Notify> mp3(secondarySerial);

//DFMiniMp3<HardwareSerial, Mp3Notify> mp3(Serial);
void waitMilliseconds(uint16_t msWait)
{
  uint32_t start = millis();
  
  while ((millis() - start) < msWait)
  {
    // calling mp3.loop() periodically allows for notifications 
    // to be handled without interrupts
    mp3.loop(); 
    delay(1);
  }
}
void configMp3(){
  //Mp3 operation
  mp3.begin();
  uint16_t volume = mp3.getVolume();
  Serial1.print("Volume ");
  Serial1.println(volume);
  mp3.setVolume(24);
  uint16_t count = mp3.getTotalTrackCount(DfMp3_PlaySource_Sd);
  Serial1.print("files ");
  Serial1.println(count);
  Serial1.println("starting...");
}
void playAudio(int audioIndex){
  configMp3();
  if(Serial){
    mp3.playMp3FolderTrack(audioIndex);  // sd:/mp3/0001.mp3
    waitMilliseconds(10000);
    mp3.end();
  }else{
    mp3.begin(9600); 
    mp3.playMp3FolderTrack(audioIndex);  // sd:/mp3/0001.mp3
    waitMilliseconds(10000);
    mp3.end();
  }
}

void loop() {
//   playAudio(1);
   readRFID();
   if(EEPROM.read(100) == 55){
    if(WiFi.status() == WL_CONNECTED){
      if (!client.connected()) {
      client.connect("esp8266-vendy4");
      String romTopic=read_String(55);
      Serial1.print("Topic in Loop: ");
      Serial1.println(romTopic);
      const char *topic = romTopic.c_str();
      client.subscribe(topic);
      readRFID();
      }
      client.loop();
      readRFID();
    }else{
      user_ap();
      readRFID();
//      digitalWrite(red,LOW);
    }
  }else{
//    digitalWrite(red,LOW);
  }
  yield;
}

void testdrawbitmap(void) {
  display.clearDisplay();

  display.drawBitmap(
    (display.width()  - LOGO_WIDTH ) / 2,
    (display.height() - LOGO_HEIGHT) / 2,
    logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
  display.display();
  delay(1000);
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

void readRFID(){
  delay(10);
  if (rdm6300.update()){
    String rfId= String(rdm6300.get_tag_id(),HEX);
    Serial1.println(rfId);
    rdm6300.end();
    delay(100);
    playAudio(1);
    setOutRegister(red);
    rdm6300.begin(RDM6300_RX_PIN);
  }
}

void setOutRegister(int index)
{
   delay(50);
   digitalWrite(latchPin, LOW);
   shiftOut(dataPin, clockPin, LSBFIRST, regByte);
   digitalWrite(latchPin, HIGH);
   delay(50);
   bitSet(regByte, index);
   delay(50);
}

void postRFID(String rfid){
  DynamicJsonDocument doc(2048);
  String romTopic=read_String(55);
  JsonObject object = doc.to<JsonObject>();
  object["RfIdCardNo"] = rfid;
  object["machinceId"] = romTopic;
  object["clientId"] = "snp01018";
  String myURL = "http://vendy.store/api/0v1/nodeMcu";
  char jsonChar[100];
  serializeJson(doc, jsonChar);
  postData(myURL,jsonChar);
  delay(10);
  doc.clear();
}

void dispense(int quantity){
// Serial.begin(9600);
// delay(10);
// Serial.print(String(quantity));
// Serial1.print("Command quantity: ");
// Serial1.println(quantity);
// playAudio(2);
// Serial.end();
}

void settings_change(String response){
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, response);
    String u_ssid=String(doc["message"]["ssid"].as<char*>());
    String u_pass=String(doc["message"]["password"].as<char*>());
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
      yield();
      delay(1000);
    }
    Serial1.println();
    Serial1.print("Connected to ");
    Serial1.println(ssid);
    Serial1.print("IP Address is : ");
    Serial1.println(WiFi.localIP());
    delay(1000);
}

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

void postData(String url, String object){
  if(WiFi.status()== WL_CONNECTED){
    Serial1.print("URL: ");
    Serial1.println(url);
    Serial1.print("Request Body: ");
    Serial1.println(object);
    HTTPClient http;                                    //Declare object of class HTTPClient
    http.begin(url);
    http.setTimeout(3000);
    int httpCode = 0, count = 0;
    do{
      http.addHeader("Authorization", "Basic dmVuZHlVc2VyOlYjbmR5VVNlUg==");
      http.addHeader("Content-Type", "application/json");
      http.addHeader("Cache-Control", "no-cache");
      httpCode = http.POST(object);                       //Send the request
      Serial1.println(httpCode);                          //Print HTTP return code
      http.end();                                         //Close connection
      if(httpCode==200){
//        digitalWrite(buzzer,HIGH);
        Serial1.println(httpCode);
        delay(100);
      }
      count++;
//      digitalWrite(buzzer,LOW);
      delay(100);
    }while(httpCode < 0 && count < 2);
  }
  return;
}

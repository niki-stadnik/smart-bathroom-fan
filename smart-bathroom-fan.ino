#include <Arduino.h>
#include <WiFi.h>
#include "WebSocketsClient.h"
#include "StompClient.h"
#include "SudoJSON.h"
#include <ArduinoJson.h>
#include <BH1750.h>
#include "Adafruit_HTU21DF.h"

//encryption
//#include "mbedtls/aes.h"
//#include <stdlib.h>

#include "config.h"
const char* wlan_ssid = WIFI;
const char* wlan_password =  PASS;
const char * ws_host = HOSTPI;
const uint16_t ws_port = PORT;
const char* ws_baseurl = URL; 
bool useWSS = USEWSS;
const char * key = KEY;


// VARIABLES
WebSocketsClient webSocket;
Stomp::StompClient stomper(webSocket, ws_host, ws_port, ws_baseurl, true);
unsigned long keepAlive = 0;


unsigned long sendtimeing = 0;

const int RelayPin = 17;

WiFiClient client;

BH1750 lightMeter;
Adafruit_HTU21DF htu = Adafruit_HTU21DF();
boolean relay = false;

int countNoIdea = 0;




void setup(){
  
  pinMode(RelayPin, OUTPUT); 
  digitalWrite(RelayPin, LOW);
 
  // setup serial
  Serial.begin(115200);
  // flush it - ESP Serial seems to start with rubbish
  Serial.println();
  // connect to WiFi
  Serial.println("Logging into WLAN: " + String(wlan_ssid));
  Serial.print(" ...");
  WiFi.begin(wlan_ssid, wlan_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" success.");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  stomper.onConnect(subscribe);
  stomper.onError(error);
 // Start the StompClient
  if (useWSS) {
    stomper.beginSSL();
  } else {
    stomper.begin();
  }


  //set up sensors
  Serial.println("HTU21D-F test");
  if (!htu.begin()) {
    Serial.println("Couldn't find sensor!");
    delay(500);
    if (!htu.begin()) {
      Serial.println("Couldn't find sensor! 2");
      ESP.restart();
    }
  }
  
  lightMeter.begin();
  Serial.println(F("BH1750 Test begin")); 
}

void subscribe(Stomp::StompCommand cmd) {
  Serial.println("Connected to STOMP broker");
  stomper.subscribe("/topic/bathroomFan", Stomp::CLIENT, handleMessage);    //this is the @MessageMapping("/test") anotation so /topic must be added
  stomper.subscribe("/topic/keepAlive", Stomp::CLIENT, handleKeepAlive);
}

void error(const Stomp::StompCommand cmd) {
  Serial.println("ERROR: " + cmd.body);
}


Stomp::Stomp_Ack_t handleMessage(const Stomp::StompCommand cmd) {
  Serial.println("Got a message!");
  Serial.println(cmd.body);
  getData(cmd.body);
  return Stomp::CONTINUE;
}
Stomp::Stomp_Ack_t handleKeepAlive(const Stomp::StompCommand cmd) {
  Serial.println(cmd.body);
  keepAlive = millis();
  return Stomp::CONTINUE;
}

void loop(){
  if(millis() >= keepAlive + 600000){  //if no messages are recieved in 10min - restart esp
    ESP.restart();
    keepAlive = millis();
  }

  if(millis() >= sendtimeing + 500){
    sendData();
    sendtimeing = millis();
  }

  webSocket.loop();
}

void sendData(){
  float lux = lightMeter.readLightLevel();
  float luxR = int(lux * 100) / 100.0;
  float temp = htu.readTemperature();
  float tempR = (int(temp * 100) / 100.0);  //rounding and calibrating
  float rel_hum = htu.readHumidity();
  float humR = (int(rel_hum * 100) / 100.0);

  // Construct the STOMP message
  SudoJSON json;
  json.addPair("bathTemp", tempR);
  json.addPair("bathHum", humR);
  json.addPair("bathLight", luxR);
  json.addPair("bathFan", relay);
  
  // Send the message to the STOMP server
  stomper.sendMessage("/app/bathroomFan", json.retrive());   //this is the @SendTo anotation
}

void getData(String input){
  char string[input.length()+1];
  char out[input.length()+1];
  input.toCharArray(string, input.length()+1);
  int count = 0;
  for(int i =0; i < input.length(); i++ ) {
    if (string[i] != '\\'){
      out[i - count]=string[i];
    }else count++;
  }
  Serial.println(out);
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, out);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    Serial.print("in:");
    Serial.println(out);
    ESP.restart();
    return;
  }

  boolean dat = doc["data"];

  if(dat == true){
    digitalWrite(RelayPin, HIGH);
    relay = true;
    //sendData();
  }else{
    digitalWrite(RelayPin, LOW);
    relay = false;
    //sendData();
  }
}

/*
String encr(char in[]){
  int x = strlen(in);
  int z = x/16;
  if(x%16 != 0) z += 1;
  if(z < 1) z = 1;

  char plainTextInput[16];
  unsigned char cipherTextOutput[16];
  String out;

  int k=0;
  for(int j=0; j<z; j++){
    for(int i=0; i<16; i++){
      plainTextInput[i] = in[k];
      k++;
    }
    encrypt(plainTextInput, key, cipherTextOutput);
    for(int i=0; i<16; i++){
      char str[3];
      sprintf(str, "%02x", (int)cipherTextOutput[i]);
      out += str;
    }
  }
  return out;
}
*/
/*
String decr(String ini){
  char in[256];
  ini.toCharArray(in, 256);
  
  int x = strlen(in);
  int z = x/32;
  if(x%32 != 0) z += 1;
  if(z < 1) z = 1;

  unsigned char cipherTextOutput[16];
  unsigned char decipheredTextOutput[16];
  String out;

  int k = 0;
  int flag = 0;
  for(int j=0; j<z; j++){
    for(int i=0; i<16; i++){
      char tt[2];
      tt[0] = in[k*2];
      tt[1] = in[k*2+1];
      long decimal_answer = strtol(tt, NULL, 16); //converting HEX to DEC
      cipherTextOutput[i] = (unsigned char)decimal_answer;
      //if(k > x) cipherTextOutput[i] = '0';
      /*
      Serial.print(k);
      Serial.print(": ");
      Serial.println(cipherTextOutput[i]);
      *//*
      k++;
    }
    decrypt(cipherTextOutput, key, decipheredTextOutput);
    for(int i=0; i<16; i++){
      if((char)decipheredTextOutput[i] == '{') flag++;
      if((char)decipheredTextOutput[i] == '}') flag--;
      out += (char)decipheredTextOutput[i];
      if(flag == 0) break;
    }
  }
  //Serial.println();
  //Serial.println(out);
  //Serial.println(ini);
  return out;
}
*/

/*

void encrypt(char * plainText, char * key, unsigned char * outputBuffer){
 
  mbedtls_aes_context aes;
 
  mbedtls_aes_init( &aes );
  mbedtls_aes_setkey_enc( &aes, (const unsigned char*) key, strlen(key) * 8 );
  mbedtls_aes_crypt_ecb( &aes, MBEDTLS_AES_ENCRYPT, (const unsigned char*)plainText, outputBuffer);
  mbedtls_aes_free( &aes );
}
 
void decrypt(unsigned char * chipherText, char * key, unsigned char * outputBuffer){
 
  mbedtls_aes_context aes;
 
  mbedtls_aes_init( &aes );
  mbedtls_aes_setkey_dec( &aes, (const unsigned char*) key, strlen(key) * 8 );
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, (const unsigned char*)chipherText, outputBuffer);
  mbedtls_aes_free( &aes );
}
*/
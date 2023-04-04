#include <Arduino.h>
#include <WiFi.h>
#include "WebSocketsClient.h"
#include "StompClient.h"
#include "SudoJSON.h"
#include <BH1750.h>
#include "Adafruit_HTU21DF.h"

//debuging
#define DEBUG 0 //1 = debug messages ON; 0 = debug messages OFF

#if DEBUG == 1
#define debugStart(x) Serial.begin(x)
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debugStart(x)
#define debug(x)
#define debugln(x)
#endif

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
boolean bootFlag = false;

//Timer Interrupt
hw_timer_t *Timer0_Cfg = NULL;
void IRAM_ATTR Timer0_ISR(){
    if(bootFlag)ESP.restart();
    bootFlag = true;
}


unsigned long sendtimeing = 0;

const int RelayPin = 17;

WiFiClient client;

BH1750 lightMeter;
Adafruit_HTU21DF htu = Adafruit_HTU21DF();
boolean relay = false;




void setup(){
  
  pinMode(RelayPin, OUTPUT); 
  digitalWrite(RelayPin, LOW);


  //Timer Interrupt
  Timer0_Cfg = timerBegin(0, 80, true);
  timerAttachInterrupt(Timer0_Cfg, &Timer0_ISR, true);
  timerAlarmWrite(Timer0_Cfg, 30000000, true); //5 000 000 = 5s timer, 30 000 000us = 30s
  timerAlarmEnable(Timer0_Cfg);
  // setup serial
  debugStart(115200);
  // flush it - ESP Serial seems to start with rubbish
  debugln();
  // connect to WiFi
  debugln("Logging into WLAN: " + String(wlan_ssid));
  debug(" ...");
  WiFi.begin(wlan_ssid, wlan_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    debug(".");
  }
  debugln(" success.");
  debug("IP: "); debugln(WiFi.localIP());
  stomper.onConnect(subscribe);
  stomper.onError(error);
 // Start the StompClient
  if (useWSS) {
    stomper.beginSSL();
  } else {
    stomper.begin();
  }


  //set up sensors
  debugln("HTU21D-F test");
  if (!htu.begin()) {
    debugln("Couldn't find sensor!");
    delay(500);
    if (!htu.begin()) {
      debugln("Couldn't find sensor! 2");
      ESP.restart();
    }
  }
  
  lightMeter.begin();
  debugln("BH1750 Test begin"); 
}

void subscribe(Stomp::StompCommand cmd) {
  debugln("Connected to STOMP broker");
  stomper.subscribe("/topic/bathroomFan", Stomp::CLIENT, handleMessage);    //this is the @MessageMapping("/test") anotation so /topic must be added
  stomper.subscribe("/topic/keepAlive", Stomp::CLIENT, handleKeepAlive);
}

void error(const Stomp::StompCommand cmd) {
  debugln("ERROR: " + cmd.body);
}


Stomp::Stomp_Ack_t handleMessage(const Stomp::StompCommand cmd) {
  debugln("Got a message!");
  debugln(cmd.body);
  getData(cmd.body);
  return Stomp::CONTINUE;
}
Stomp::Stomp_Ack_t handleKeepAlive(const Stomp::StompCommand cmd) {
  debugln(cmd.body);
  keepAlive = millis();
  return Stomp::CONTINUE;
}

void loop(){
  if(millis() >= keepAlive + 60000){  //if no messages are recieved in 1min - restart esp
    ESP.restart();
  }

  bootFlag = false;
  
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

/*
  //free heap
  SudoJSON jsonH;
  int freeH = ESP.getFreeHeap();
  jsonH.addPair("freeH", freeH);
  stomper.sendMessage("/app/client", jsonH.retrive());
  */

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
  SudoJSON json = SudoJSON(input);
  boolean dat = json.getPairB("data");

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
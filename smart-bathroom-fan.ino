#include <ArduinoJson.h>
#include <WiFi.h>
#include <Wire.h>
#include <BH1750.h>
#include "Adafruit_HTU21DF.h"

//encryption
#include "mbedtls/aes.h"
#include <stdlib.h>

#include "config.h"


const char* ssid = WIFI;
const char* password =  PASS;
 
const char * hostpi = HOSTPI;
const char * hosttest = HOSTTEST;
const char * host;
const uint16_t port = PORT;

char * key = KEY;

unsigned long sendtimeing = 0;

const int RelayPin = 17;
const int TestMode = 23;

WiFiClient client;

BH1750 lightMeter;
Adafruit_HTU21DF htu = Adafruit_HTU21DF();
boolean relay = false;

int countNoIdea = 0;




void setup()
{
  pinMode(RelayPin, OUTPUT); 
  pinMode(TestMode, INPUT_PULLUP);

  digitalWrite(RelayPin, LOW);
 
  //set up coms
  Serial.begin(115200);
 
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("...");
  }
  Serial.print("WiFi connected with IP: ");
  Serial.println(WiFi.localIP());


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

  if(digitalRead(TestMode) == HIGH){
    //host = hostpi;
    host = hosttest;
    Serial.println("normal mode");
  }else{
    host = hosttest;
    Serial.println("test mode");
  }
  
  lightMeter.begin();
  Serial.println(F("BH1750 Test begin")); 
}

void loop()
{
  if(!client.connected()){
    if (!client.connect(host, port)) {  
    Serial.println("Connection to host failed");
    delay(1000);
    return;
    }
    Serial.println("Connected to server successful!");
  }

  if(millis() >= sendtimeing + 500){
    sendData();
    sendtimeing = millis();
  }

  

  String line = client.readStringUntil('\r');
  if(line != NULL && line.length() > 5){ // was > 5
    Serial.println(line.length());
    getData(line);
  }
}

void sendData(){
  float lux = lightMeter.readLightLevel();
  float luxR = int(lux * 100) / 100.0;
  float temp = htu.readTemperature();
  float tempR = (int(temp * 100) / 100.0) - 1.5;  //rounding and calibrating
  float rel_hum = htu.readHumidity();
  float humR = (int(rel_hum * 100) / 100.0) + 15.74;

  DynamicJsonDocument doc(96);
  doc["ID"] = 1;
  JsonObject dat = doc.createNestedObject("data");
  dat["bathTemp"] = tempR;
  dat["bathHum"] = humR;
  dat["bathLight"] = luxR;
  dat["bathFan"] = relay;
  
  char json[96];
  serializeJson(doc, json);

  String out = encr(json);  //encripting the json char array

  client.println(out);
}

void getData(String input){
  String dec = decr(input);
  Serial.println("Decoded: ");
  Serial.println(dec);

  char firstChar = dec.charAt(0);
  //Serial.println(firstChar);
  if (firstChar != '{') {
    countNoIdea++;
    if (countNoIdea >= 5){
      Serial.println("5 unknown messages");
      ESP.restart();
    }
    return;   //ensures that the json is decoded correctly
  }
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, dec);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    Serial.print("in:");
    Serial.println(input);
    ESP.restart();
    return;
  }
  
  countNoIdea = 0;

  int id = doc["ID"]; // 1000
  boolean dat = doc["data"];
  
  if(dat == true){
    digitalWrite(RelayPin, HIGH);
    relay = true;
    sendData();
  }else{
    digitalWrite(RelayPin, LOW);
    relay = false;
    sendData();
  }
  
  //Serial.println(id);
  //Serial.println(dat);  
}


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
      */
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

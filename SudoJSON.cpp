#include "SudoJSON.h"
#include <Arduino.h> 


SudoJSON::SudoJSON(){ 
}
void SudoJSON::check(){
  int length = message.length();
  if(length > 5){
    message[length - 1] = ',';
  }
}
void SudoJSON::addPair(String name, const char value[]){
  check();
  message += "\\\"";
  message += name;
  message += "\\\":\\\"";
  message += value;
  message += "\\\"}";
}
void SudoJSON::addPair(String name, int value){
  check();
  message += "\\\"";
  message += name;
  message += "\\\":";
  message += value;
  message += "}";
}
void SudoJSON::addPair(String name, float value){
  check();
  message += "\\\"";
  message += name;
  message += "\\\":";
  message += value;
  message += "}";
}
void SudoJSON::addPair(String name, boolean value){
  check();
  message += "\\\"";
  message += name;
  message += "\\\":";
  message += value;
  message += "}";
}
String SudoJSON::retrive(){
  return message;
}
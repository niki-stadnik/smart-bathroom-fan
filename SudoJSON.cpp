#include "SudoJSON.h"
#include <Arduino.h> 


SudoJSON::SudoJSON(){ 
}
SudoJSON::SudoJSON(String string){
  int len = string.length() + 1;
  char input[len];
  string.toCharArray(input, len);

  int countCr = 0;
  int wordStart = 0;
  boolean wordFound = false;

  for(int i = 0; i < len; i++){
    if (input[i] == ':'){
      if (input[i+1] != '\\'){
        wordFound = true;
        wordStart = i + 1;
      }
      continue;
    }
    if (wordFound){
      if (input[i] == ',' || input[i] == '}'){
        set[countAr][countCr] = '\0';
        //chars[countAr] = countCr + 2;
        wordFound = false;
        countAr++;
        countCr = 0;
        continue;
      }
    }
    if (input[i] == '\\'){
      if (wordFound){
        set[countAr][countCr] = '\0';
        //chars[countAr] = countCr + 2;
        wordFound = false;
        countAr++;
        countCr = 0;
      } else{
        wordFound = true;
        wordStart = i + 2;
      }
      continue;
    }
    if (wordFound && wordStart <= i){
      set[countAr][countCr] = input[i];
      countCr++;
      chars[countAr] = countCr;
    }
  }
}



String SudoJSON::find(String value){
  int siz = value.length();
  boolean flag = false;
  for (int i = 0; i < countAr; i = i + 2){
    if (siz != chars[i]) continue;
    flag = true;
    for (int j = 0; j < siz; j++){
      if (value[j] != set[i][j]) {
        flag = false;
        break;
      }
    }
    if(flag) return set[i + 1];
  }
  return "nope";
}
String SudoJSON::getPairS(String value){
  String str = find(value);
  return str;
}
float SudoJSON::getPairF(String value){
  String str = find(value);
  float ff = str.toFloat();
  return ff;
}
int SudoJSON::getPairI(String value){
  String str = find(value);
  int i = str.toInt();
  return i;
}
boolean SudoJSON::getPairB(String value){
  String str = find(value);
  boolean state = false;
  if(str == "true") state = true;
  return state;
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





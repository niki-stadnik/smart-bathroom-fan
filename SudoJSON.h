#include "WString.h"
#ifndef SudoJSON_h
#define SudoJSON_h
#include <Arduino.h> 

class SudoJSON{
  public:
    SudoJSON();
    SudoJSON(String input);
    void check();
    void addPair(String name, const char value[]);
    void addPair(String name, float value);
    void addPair(String name, int value);
    void addPair(String name, boolean value);
    String retrive();
    String getPairS(String value);
    float getPairF(String value);
    int getPairI(String value);
    boolean getPairB(String value);
  private:
    String find(String value);
    String message = "{";
    char set[40][20];
    int chars[40];
    int countAr = 0;
};

#endif
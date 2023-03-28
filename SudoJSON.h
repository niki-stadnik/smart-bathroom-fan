#include "WString.h"
#ifndef SudoJSON_h
#define SudoJSON_h
#include <Arduino.h> 

class SudoJSON{
  public:
    SudoJSON();
    void check();
    void addPair(String name, const char value[]);
    void addPair(String name, float value);
    void addPair(String name, int value);
    void addPair(String name, boolean value);
    String retrive();
  private:
    String message = "{";
};

#endif
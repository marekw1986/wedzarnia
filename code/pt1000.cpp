#include <Arduino.h>
#include "pt1000.h"

pt1000::pt1000 (unsigned char pin) {
  _pin = pin;
  _i = 0;
}


void pt1000::read (void) {
  _raw[_i] = analogRead(_pin);
  _i++;
  if (_i >= 20) _i=0;
}



float pt1000::degC (void) {
  int raw = 0;
  unsigned char i;

  for (i=0; i<20; i++) {
    raw += _raw[i];
  }
  raw /= 20;
  
  float voltage = raw * (5.0 / 1023.0);
  float rpt = ((5.0-voltage)/voltage) * 3900;
  float temp = (rpt - 1000)/3.9;
  return temp;
}



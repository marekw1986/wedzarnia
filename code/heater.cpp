#include <Arduino.h>
#include <avr/io.h>
#include <util/atomic.h>
#include "heater.h"


/* Klasa Heater */

volatile byte duty_cycle = 0;
volatile byte temp;

HeaterClass::HeaterClass (void) {
  DDRC |= (1<<PC0);
  PORTC &= ~(1<<PC0);
  _minPower = 0;
  _maxPower = 100;
}

void HeaterClass::start (void) {
  TCNT2 = 0;
  TCCR2A |= (1 << WGM21); // CTC Mode
  TCCR2B |= (1 << CS22) | (1 << CS21) | (1 << CS20);  // FCPU/1024
  TIMSK2 |= (1 << OCIE2A); // CTC interrupt
  OCR2A = 155;
}

void HeaterClass::stop (void) {
  TCCR2A &= ~(1 << WGM21); // CTC Mode
  TCCR2B &= ~((1 << CS22) | (1 << CS21) | (1 << CS20));  // FCPU/1024
  TIMSK2 &= ~(1 << OCIE2A); // CTC interrupt;
  PORTC &= ~(1<<PC0);    
}

void HeaterClass::setPower (byte power) {

  if (power > _maxPower) {
    power = _maxPower;
  }
  else if (power < _minPower) {
    power = _minPower;
  }
  
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    duty_cycle = temp = power;
  }
}

void HeaterClass::increasePower (byte value) {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if ( (temp + value) <= _maxPower ) temp += value;
  }
}

void HeaterClass::decreasePower (byte value) {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if ( (temp - value) >= _minPower ) temp -= value;
  }
}

void HeaterClass::setMaxPower (byte value) {
  if ( (value > 0) && (value <=100) ) _maxPower = value;
}

void HeaterClass::setMinPower (byte value) {
  if ( (value >= 0) && (value < 100) ) _minPower = value;
}

byte HeaterClass::getMaxPower (void) {
  return _maxPower;
}

byte HeaterClass::getMinPower (void) {
  return _minPower;
}


ISR(TIMER2_COMPA_vect) {
  static byte counter = 0;

  if (counter >= 100) {
    counter = 0;
    duty_cycle = temp;
    PORTC |= (1<<PC0);
  }

  if (counter == duty_cycle) {
    PORTC &= ~(1<<PC0);
  }

  counter++;
}

HeaterClass Heater;

#ifndef HEATER_H
#define HEATER_H

class HeaterClass {

  public:
    HeaterClass (void);
    void start (void);
    void stop (void);
    void setPower (byte power);
    void increasePower (byte value);
    void decreasePower (byte value);
    void setMaxPower (byte value);
    void setMinPower (byte value);
    byte getMaxPower (void);
    byte getMinPower (void);
  private:
    byte _maxPower;
    byte _minPower;
};

extern HeaterClass Heater;

#endif

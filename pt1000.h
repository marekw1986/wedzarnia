#ifndef PT1000_H
#define PT1000_H

class pt1000 {
  public:
    pt1000 (unsigned char pin);
    void read (void);
    float degC (void);
  private:
    unsigned char _pin;
    unsigned int _raw[20];
    unsigned char _i;
};

#endif

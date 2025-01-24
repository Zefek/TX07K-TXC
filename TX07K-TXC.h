/*  TX07K-TXC - Library for reading data from TX07K-TXC temperature sensor
    Created by Petr B. at January 2025
*/
#ifndef TX07K_TXC_H
#define TX07K_TXC_H

#include <Arduino.h>

#define RING_BUFFER_SIZE  256

#define SYNC_LENGTH  8000
#define SEP_LENGTH   500
#define BIT1_LENGTH  4000
#define BIT0_LENGTH  2000

class TX07KTXC
{
  private:
    static unsigned long timings[RING_BUFFER_SIZE];
    static unsigned int syncIndex1;
    static unsigned int syncIndex2;
    static bool received;
    static uint8_t interruptPin;
    static unsigned long receivedMillis;
    static bool isSync(unsigned int idx);
    static void handler();
    static void (*TemperatureChanged)(double, uint8_t, uint8_t, uint8_t*, bool);
    static bool Read(byte *bytes);
    static unsigned long CheckCRC(byte *bytes, int);
    static uint8_t enablePin;

  public:
    TX07KTXC(uint8_t interruptPin, uint8_t enablePin, void (*temperatureChanged)(double, uint8_t, uint8_t, uint8_t*, bool));
    void Init();
    void CheckTemperature();
};
#endif

#include "TX07K-TXC.h"

void OutsideTemperatureChanged(double temperature, uint8_t channel, uint8_t sensorId, uint8_t* rawData, bool transmitedByButton);

TX07KTXC outsideTemperatureSensor(2, 3, OutsideTemperatureChanged);

void OutsideTemperatureChanged(double temperature, uint8_t channel, uint8_t sensorId, uint8_t* rawData, bool transmitedByButton)
{
}

void setup()
{
  outsideTemperatureSensor.Init();
}

void loop()
{
  outsideTemperatureSensor.CheckTemperature();
}
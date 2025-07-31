# TX07K-TXC Arduino Library

This Arduino library enables communication with TX07K/TXC wireless temperature and humidity sensors. It provides an interrupt-driven interface for receiving and decoding data packets transmitted by these sensors.

## Features

- **Interrupt-driven signal decoding:** Captures sensor data with high timing accuracy using hardware interrupts.
- **Automatic data parsing:** Decodes raw RF pulses into temperature, humidity, channel, and sensor ID data.
- **Data integrity checks:** Employs synchronization pattern detection and CRC validation to ensure correct data reception.
- **Callback mechanism:** Notifies user code with parsed temperature and humidity data via a user-supplied callback function.

## Usage

1. **Include the library:**
   ```cpp
   #include "TX07K-TXC.h"
   ```

2. **Define a callback function:**
   ```cpp
   void onTemperatureChanged(double temperature, uint8_t channel, uint8_t sensorId, uint8_t* rawData, bool byButton) {
     // Handle new measurements here
   }
   ```

3. **Instantiate the sensor object:**
   ```cpp
   TX07KTXC sensor(interruptPin, enablePin, onTemperatureChanged);
   ```

4. **Initialize in `setup()`:**
   ```cpp
   void setup() {
     sensor.Init();
   }
   ```

5. **Regularly check for new data in `loop()`:**
   ```cpp
   void loop() {
     sensor.CheckTemperature();
   }
   ```

## Notes

- Only one instance of `TX07KTXC` should be used at a time due to static member variables.
- Timing constants may need adjustment for sensors with different transmission timings.
- The callback provides temperature (Celsius), humidity, channel, sensor ID, raw data, and a flag for button-triggered transmission.

## Files

- **TX07K-TXC.h:** Class declaration, configuration constants, and API.
- **TX07K-TXC.cpp:** Implementation of sensor reading, decoding, and event handling.

## License

See project for license details.

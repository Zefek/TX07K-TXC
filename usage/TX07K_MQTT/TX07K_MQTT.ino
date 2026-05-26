#include <TX07K-TXC.h>
#include <EspDrv.h>
#include <MQTTClient.h>
#include <SoftwareSerial.h>
#include "config.h"

#define RX_PIN 4
#define TX_PIN 5

//TX07K_TXC/<sensorId>/<channel>/[temperature|humidity|dewPoint|absoluteHumidity|sensorId|channel|batteryLow|temperatureDown|temperatureUp|forcedTransmision|rawData]
const char temperatureTopicFormat[] PROGMEM = "TX07K_TXC/%u/%u/temperature";
const char humidityTopicFormat[] PROGMEM = "TX07K_TXC/%u/%u/humidity";
const char dewPointTopicFormat[] PROGMEM = "TX07K_TXC/%u/%u/dewPoint";
const char absoluteHumidityTopicFormat[] PROGMEM = "TX07K_TXC/%u/%u/absoluteHumidity";
const char sensorIdTopicFormat[] PROGMEM = "TX07K_TXC/%u/%u/sensorId";
const char channelTopicFormat[] PROGMEM = "TX07K_TXC/%u/%u/channel";
const char batteryLowTopicFormat[] PROGMEM = "TX07K_TXC/%u/%u/batteryLow";
const char temperatureDownTopicFormat[] PROGMEM = "TX07K_TXC/%u/%u/temperatureDown";
const char temperatureUpTopicFormat[] PROGMEM = "TX07K_TXC/%u/%u/temperatureUp";
const char forcedTransmisionTopicFormat[] PROGMEM = "TX07K_TXC/%u/%u/forcedTransmision";
const char rawDataTopicFormat[] PROGMEM = "TX07K_TXC/%u/%u/rawData";

//Single-slot buffer between the RF callback and the publish step in loop().
//If a new frame arrives while pending is still true, it is dropped — the older
//unpublished reading wins. Acceptable for slow temperature data.
typedef struct
{
  bool pending;
  double temperature;
  uint8_t channel;
  uint8_t sensorId;
  uint8_t rawData[5];
  bool transmitedByButton;
} sensorReading_t;

void OutsideTemperatureChanged(double temperature, uint8_t channel, uint8_t sensorId, uint8_t* rawData, bool transmitedByButton);
void MQTTMessageReceive(char* topic, uint8_t* payload, uint16_t length) {  }

//Magnus formula, accurate for 0..60 C, 1..100 %RH.
double DewPoint(double t, double rh)
{
  const double a = 17.62;
  const double b = 243.12;
  double gamma = (a * t) / (b + t) + log(rh / 100.0);
  return (b * gamma) / (a - gamma);
}

//Absolute humidity in g/m^3. Uses the same Magnus constants as DewPoint
//for a consistent saturation vapour pressure model.
double AbsoluteHumidity(double t, double rh)
{
  const double a = 17.62;
  const double b = 243.12;
  double es = 6.112 * exp((a * t) / (b + t));
  return es * rh * 2.1674 / (273.15 + t);
}

TX07KTXC outsideTemperatureSensor(2, 3, OutsideTemperatureChanged);
MQTTConnectData mqttConnectData = { MQTTHost, MQTTPort, MQTTClientId, MQTTUser, MQTTPassword, "", 0, false, "", false, 0x0 }; 

SoftwareSerial softwareSerial(RX_PIN, TX_PIN);
EspDrv drv(&softwareSerial);
MQTTClient client(&drv, MQTTMessageReceive);

unsigned long currentMillis = 0;
unsigned long mqttLastConnectionTry = 0;
unsigned long mqttConnectionTimeout = 0;
sensorReading_t sensorReading = { false };

void MQTTConnect()
{
  if(currentMillis - mqttLastConnectionTry < mqttConnectionTimeout)
  {
    return;
  }
  int wifiStatus = drv.GetConnectionStatus();
  Serial.print(F("Wifi status "));
  Serial.println(wifiStatus);
  bool wifiConnected = wifiStatus == WL_CONNECTED;
  if(wifiStatus == WL_DISCONNECTED || wifiStatus == WL_IDLE_STATUS)
  {
    wifiConnected = drv.Connect(WifiSSID, WifiPassword);
    mqttLastConnectionTry = currentMillis;
  }
  if(wifiConnected)
  {
    bool isConnected = client.IsConnected();
    if(!isConnected)
    {
      Serial.println(F("Connect"));
      if(client.Connect(mqttConnectData))
      {
        mqttLastConnectionTry = currentMillis;
        mqttConnectionTimeout = 0;
      }
      else
      {
        mqttLastConnectionTry = currentMillis;
        mqttConnectionTimeout = min(mqttConnectionTimeout * 2 + random(0, 5000), 300000);
      }
    }
  }
  else
  {
    mqttLastConnectionTry = currentMillis;
    mqttConnectionTimeout = min(mqttConnectionTimeout * 2 + random(5000, 30000), 300000);
  }
}

//Stays minimal — runs while the RF interrupt is detached. Copy data and exit.
void OutsideTemperatureChanged(double temperature, uint8_t channel, uint8_t sensorId, uint8_t* rawData, bool transmitedByButton)
{
  if(sensorReading.pending)
  {
    //Previous reading hasn't been published yet, drop the new one.
    return;
  }
  sensorReading.temperature = temperature;
  sensorReading.channel = channel;
  sensorReading.sensorId = sensorId;
  memcpy(sensorReading.rawData, rawData, 5);
  sensorReading.transmitedByButton = transmitedByButton;
  sensorReading.pending = true;
}

void PublishSensorData(const sensorReading_t& reading)
{
  uint8_t humidity = (reading.rawData[3] & 0x0F) * 10 + ((reading.rawData[4] & 0xF0) >> 4);
  bool batteryLow = (reading.rawData[1] & 0x04) != 0;
  bool temperatureDown = (reading.rawData[1] & 0x02) != 0;
  bool temperatureUp = (reading.rawData[1] & 0x01) != 0;

  char topic[48];
  char payload[16];

  sprintf_P(topic, temperatureTopicFormat, reading.sensorId, reading.channel);
  dtostrf(reading.temperature, 0, 2, payload);
  client.Publish(topic, payload);

  sprintf_P(topic, humidityTopicFormat, reading.sensorId, reading.channel);
  sprintf(payload, "%u", humidity);
  client.Publish(topic, payload);

  sprintf_P(topic, dewPointTopicFormat, reading.sensorId, reading.channel);
  dtostrf(DewPoint(reading.temperature, humidity), 0, 2, payload);
  client.Publish(topic, payload);

  sprintf_P(topic, absoluteHumidityTopicFormat, reading.sensorId, reading.channel);
  dtostrf(AbsoluteHumidity(reading.temperature, humidity), 0, 2, payload);
  client.Publish(topic, payload);

  sprintf_P(topic, sensorIdTopicFormat, reading.sensorId, reading.channel);
  sprintf(payload, "%u", reading.sensorId);
  client.Publish(topic, payload);

  sprintf_P(topic, channelTopicFormat, reading.sensorId, reading.channel);
  sprintf(payload, "%u", reading.channel);
  client.Publish(topic, payload);

  sprintf_P(topic, batteryLowTopicFormat, reading.sensorId, reading.channel);
  client.Publish(topic, batteryLow ? "1" : "0");

  sprintf_P(topic, temperatureDownTopicFormat, reading.sensorId, reading.channel);
  client.Publish(topic, temperatureDown ? "1" : "0");

  sprintf_P(topic, temperatureUpTopicFormat, reading.sensorId, reading.channel);
  client.Publish(topic, temperatureUp ? "1" : "0");

  sprintf_P(topic, forcedTransmisionTopicFormat, reading.sensorId, reading.channel);
  client.Publish(topic, reading.transmitedByButton ? "1" : "0");

  sprintf_P(topic, rawDataTopicFormat, reading.sensorId, reading.channel);
  client.Publish(topic, reading.rawData, 5);

  //Send raw data directly to serial.
  Serial.write(reading.rawData, 5);
  Serial.write("\r\n");
}

void setup()
{
  Serial.begin(57600);
  softwareSerial.begin(57600);
  drv.Init(32);
  drv.Connect(WifiSSID, WifiPassword);
  outsideTemperatureSensor.Init();
  Serial.println(F("Setup OK"));
}

void loop()
{
  currentMillis = millis();
  if(!client.Loop())
  {
    MQTTConnect();
  }
  outsideTemperatureSensor.CheckTemperature();
  if(sensorReading.pending && client.IsConnected())
  {
    PublishSensorData(sensorReading);
    sensorReading.pending = false;
  }
}
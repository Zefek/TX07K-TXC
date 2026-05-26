# TX07K-TXC → MQTT bridge

Example sketch that receives data from one or more TX07K/TXC wireless temperature & humidity sensors, decodes the payload, computes a few derived values (dew point, absolute humidity) and publishes everything to an MQTT broker through an ESP8266 module running AT firmware.

The sketch is meant as a starting point: it ties together the [TX07K-TXC](../../src) and [MQTTESP8266](../MQTTESP8266) libraries from this repo and shows how to wire the callback from the RF receiver to MQTT topics. It is small enough to fit on an Arduino Uno (ATmega328P, 32 KB flash / 2 KB SRAM).

## Hardware

- Arduino Uno (or any AVR board with at least one external interrupt and SoftwareSerial-capable pins).
- 433 MHz superheterodyne receiver (the kind sold for TX07K decoding). Connect its data output to **D2** (external interrupt pin).
- ESP8266 module flashed with AT firmware (ESP-01 is enough), 3.3 V powered with a proper level shifter on the RX line.
- TX07K/TXC sensor(s) — up to three channels per sensor ID, multiple sensor IDs supported transparently.

### Pin map

| Signal                       | Arduino pin | Notes                                       |
| ---------------------------- | ----------- | ------------------------------------------- |
| RF receiver DATA → MCU       | D2          | External interrupt 0, set in sketch         |
| RF receiver power enable     | D3          | Driven HIGH in `TX07KTXC::Init()`           |
| ESP8266 TX → MCU (SoftSerial RX) | D4      | `RX_PIN` in sketch                          |
| ESP8266 RX ← MCU (SoftSerial TX) | D5      | `TX_PIN` in sketch — use level shifter      |
| Serial (USB)                 | D0/D1       | 57600 baud, debug + raw data dump           |

## Configuration

The sketch reads credentials from [`config.h`](config.h), which ships with placeholder values:

```cpp
#define WifiSSID     "ssid"
#define WifiPassword "wifipassword"
#define MQTTHost     "mqtthost"
#define MQTTUser     "mqttuser"
#define MQTTPassword "mqttpassword"
#define MQTTClientId "mqttclientid"
#define MQTTPort     1883
```

This is an example sketch — the intended workflow is **fork the repository, edit `config.h` directly with your own values, then build and deploy from the fork**. There is no separate `config_default.h` template and no `.gitignore` rule that would hide `config.h` from git. That keeps the setup short (one file to edit) at the cost of one obligation:

> **Do not push the populated `config.h` back to a public fork** — it contains your Wi-Fi and MQTT credentials. If your fork is public, either keep your changes only in a local branch you never push, mark the file `git update-index --assume-unchanged usage/TX07K_MQTT/config.h` after editing, or move to a private fork.

## Build & upload

Build profile is declared in [`sketch.yaml`](sketch.yaml). With `arduino-cli`:

```
arduino-cli compile --profile TX07K-TXC_MQTT_Uno usage/TX07K_MQTT
arduino-cli upload  --profile TX07K-TXC_MQTT_Uno -p COM3 usage/TX07K_MQTT
```

The profile points at `../../src` (the TX07K-TXC library) and `../MQTTESP8266` (the MQTT/ESP driver), so nothing else needs to be installed.

## MQTT topics

Every measurement publishes to topics under the prefix `TX07K_TXC/<sensorId>/<channel>/...`. One topic per attribute (no JSON) — chosen deliberately, JSON on a 2 KB SRAM Uno is risky.

| Topic suffix       | Payload         | Notes                                                                   |
| ------------------ | --------------- | ----------------------------------------------------------------------- |
| `temperature`      | float, °C (2 d.p.) | Decoded from the sensor packet                                       |
| `humidity`         | uint, %         | BCD decoded from raw nibbles                                            |
| `dewPoint`         | float, °C       | Computed via Magnus formula                                             |
| `absoluteHumidity` | float, g/m³     | Computed from temperature & RH                                          |
| `sensorId`         | uint            | TX07K sensor ID (8-bit, regenerated on battery replacement)             |
| `channel`          | uint            | 1–3                                                                     |
| `batteryLow`       | `0` / `1`       | Battery low flag from raw byte 1                                        |
| `temperatureDown`  | `0` / `1`       | Trend flag                                                              |
| `temperatureUp`    | `0` / `1`       | Trend flag                                                              |
| `forcedTransmision`| `0` / `1`       | Set when the user pressed the TX button on the sensor                   |
| `rawData`          | 5-byte binary   | Raw decoded frame, useful for debugging or alternative integrations     |

Wildcard subscription examples:

```
TX07K_TXC/+/+/temperature      # all temperatures from all sensors/channels
TX07K_TXC/123/+/+              # everything from sensor 123
```

## Home Assistant integration

The topics above can be consumed by Home Assistant either through static MQTT entities in `configuration.yaml`, or via MQTT discovery. Before picking either approach, please read the **Important: sensor ID is not stable** section below.

### Option A — static `configuration.yaml`

Suitable when you have a small, known set of sensors and don't mind editing YAML after a battery change.

```yaml
mqtt:
  sensor:
    - name: "Outdoor temperature"
      unique_id: tx07k_123_1_temperature
      state_topic: "TX07K_TXC/123/1/temperature"
      unit_of_measurement: "°C"
      device_class: temperature
      state_class: measurement
    - name: "Outdoor humidity"
      unique_id: tx07k_123_1_humidity
      state_topic: "TX07K_TXC/123/1/humidity"
      unit_of_measurement: "%"
      device_class: humidity
      state_class: measurement
    - name: "Outdoor dew point"
      unique_id: tx07k_123_1_dewpoint
      state_topic: "TX07K_TXC/123/1/dewPoint"
      unit_of_measurement: "°C"
      device_class: temperature
      state_class: measurement
    - name: "Outdoor absolute humidity"
      unique_id: tx07k_123_1_abshumidity
      state_topic: "TX07K_TXC/123/1/absoluteHumidity"
      unit_of_measurement: "g/m³"
      state_class: measurement

  binary_sensor:
    - name: "Outdoor sensor battery low"
      unique_id: tx07k_123_1_battery
      state_topic: "TX07K_TXC/123/1/batteryLow"
      payload_on: "1"
      payload_off: "0"
      device_class: battery
```

Replace `123` (sensor ID) and `1` (channel) with the values you observe on the `sensorId` / `channel` topics for your physical sensor.

### Option B — MQTT discovery

Publish a retained discovery message under `homeassistant/<component>/<unique>/config` and Home Assistant will auto-create the entities. The bridge currently does not emit discovery messages itself (it would not fit cleanly on the Uno's flash/SRAM budget), so you publish them once from an external tool — `mosquitto_pub`, a Node-RED flow, or any small helper — when you wire up a new sensor.

Example for a temperature entity:

Topic: `homeassistant/sensor/tx07k_123_1_temperature/config`  
Retain: `true`  
Payload:

```json
{
  "name": "Outdoor temperature",
  "unique_id": "tx07k_123_1_temperature",
  "state_topic": "TX07K_TXC/123/1/temperature",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "state_class": "measurement",
  "device": {
    "identifiers": ["tx07k_123_1"],
    "name": "TX07K sensor 123 / channel 1",
    "manufacturer": "Hideki",
    "model": "TX07K-TXC"
  }
}
```

Example for the battery binary sensor:

Topic: `homeassistant/binary_sensor/tx07k_123_1_battery/config`

```json
{
  "name": "Battery low",
  "unique_id": "tx07k_123_1_battery_low",
  "state_topic": "TX07K_TXC/123/1/batteryLow",
  "payload_on": "1",
  "payload_off": "0",
  "device_class": "battery",
  "device": { "identifiers": ["tx07k_123_1"] }
}
```

Publishing the same `device.identifiers` from each entity groups them under a single device in Home Assistant. To remove a discovered entity later, publish an empty retained payload to the same `…/config` topic.

### Important: sensor ID is not stable

TX07K/TXC sensors **regenerate a new 8-bit ID every time the battery is inserted**. The channel (1–3) is set by the physical switch on the sensor and stays the same — the sensor ID does not.

What this means in practice:

- A static `state_topic: TX07K_TXC/123/1/temperature` will go silent after a battery change because the sensor now publishes under, for example, `TX07K_TXC/57/1/temperature`.
- The same applies to discovery: the old discovered entity becomes orphaned, and a new entity for the new ID will appear (if you republish discovery for it).

Possible mitigations, in order of effort:

1. **Manual relabel.** After a battery change, watch the `TX07K_TXC/+/<channel>/sensorId` topics, identify the new ID, and update your YAML or republish discovery. Simple, but requires a maintenance step.
2. **Channel-only republish.** Run a small bridge service (Node-RED, a Python script, an HA `mqtt.publish` automation) that subscribes to `TX07K_TXC/+/<channel>/...` and re-publishes under a stable prefix such as `home/outdoor/temperature`. Home Assistant then consumes the stable topic and is unaffected by ID changes. Requires that you only have one sensor per channel.
3. **Automate discovery republish.** Have the helper from #2 also (re)publish discovery messages whenever it sees a new sensor ID for a known channel, removing the previous entity by publishing an empty payload to its old `…/config` topic.

Whatever path you pick, design your dashboards and automations around the *channel*, not the *sensor ID*, so they survive a battery swap with minimal fuss.

### Reference implementation: ArduinoSerialReader

A more complete answer to all of the above lives in a sibling project — [Zefek/ArduinoSerialReader](https://github.com/Zefek/ArduinoSerialReader), specifically the [`ReadFromMQTT`](https://github.com/Zefek/ArduinoSerialReader/tree/ReadFromMQTT) branch. It is a .NET background service that consumes the data this bridge produces and turns it into a polished Home Assistant integration. The relevant ideas, if you want to build something similar:

- **Stable `Room` abstraction.** Rooms are persisted in a database with a `Name` (stable, user-meaningful, e.g. `"living-room"`) and a `SensorName` of the form `"<sensorId>_<channel>"`. After a battery change the new `Id_Channel` is written to `SensorNewName`, the service performs the renaming sweep, and the dashboards keep working without any HA edit. HA itself never sees the volatile sensor ID — only the room identity.
- **HA MQTT discovery is published by the service**, not by the Uno. On startup, `SendAllSensorsDiscovery()` walks all rooms and publishes one `homeassistant/<component>/<sensorName>_<kind>/config` message per attribute (temperature, humidity, dew point, absolute humidity, battery, trends, derived window-open flag). State is consolidated into a single JSON topic `TX07KTXC/<sensorName>/state` with `value_template` extraction, plus `expire_after: 600` so entities go `unavailable` if a sensor stops talking.
- **Two-way binding to HA's device registry.** A WebSocket listener subscribes to `device_registry_updated` events, so renaming a device or assigning it to an area in the Home Assistant UI is reflected back into the service's room database. The configuration stays consistent regardless of which side initiated the change.
- **Additional computed values.** Beyond what this Arduino bridge publishes, the service adds exponential moving averages, temperature/humidity *trend* (°C/h, %/h), and a heuristic *window-open* detection — all data points HA can graph or use in automations.

The `ReadFromMQTT` branch consumes the 5-byte `rawData` topic published by this sketch directly — payload layouts are aligned, so the bridge can be used as a drop-in source for the service.

## Derived values

Both formulas use `<math.h>` (`log`, `exp`) and operate on `double` (single-precision on AVR). They share the same Magnus constants (`a = 17.62, b = 243.12`) so the saturation vapour pressure is modelled consistently across both outputs.

**Dew point** — Magnus formula, accurate roughly within ±0.4 °C in the 0–60 °C / 1–100 % RH range:

```
γ  = (a · T) / (b + T) + ln(RH / 100)
Td = (b · γ) / (a − γ)
```

**Absolute humidity** — vapour mass per volume of air, in g/m³:

```
es = 6.112 · exp((a · T) / (b + T))
AH = es · RH · 2.1674 / (273.15 + T)
```

## Memory notes

| Resource | Used    | Notes                                       |
| -------- | ------- | ------------------------------------------- |
| Flash    | ~22 KB  | Out of 32 KB                                |
| SRAM     | 1710 B  | Out of 2048 B → ~338 B free for stack       |

To stay below the SRAM ceiling, all topic format strings and Serial debug literals are kept in flash (`PROGMEM` + `sprintf_P`, `F()` macro). The two stack buffers in the publish callback (`char topic[48]`, `char payload[16]`) are the bulk of the runtime stack usage. If you add more publish blocks, keep an eye on the free SRAM figure reported by `arduino-cli compile`.

## Customisation hooks

- **What to publish — start here.** For most integrations you only need to touch `PublishSensorData()`. Add, remove or reformat topics there (e.g. drop attributes you don't need, switch to JSON, set the retain flag, add a site prefix). If you also need to propagate additional data from the RF callback into the publish step — RSSI, an arrival timestamp for `expire_after`, a retry counter — add the corresponding field to `sensorReading_t` and fill it in `OutsideTemperatureChanged()`. Everything else (decode, MQTT reconnect, memory layout) is infrastructure that should not need to change.
- **Publish lifecycle.** Decoding and publishing are decoupled: the RF callback only copies the frame into a single-slot `sensorReading_t` buffer and sets `pending = true`. The actual publish runs from `loop()` once `client.IsConnected()` is satisfied. If a second frame arrives before the previous one was published, the new one is dropped — at the cadence TX07K transmits (every ~30–60 s) this almost never happens, but if you need lossless behaviour, replace the single slot with a small ring buffer.
- **Connection backoff.** `MQTTConnect()` uses exponential backoff with jitter (capped at 5 min). Tweak `mqttConnectionTimeout` arithmetic if your broker has different retry expectations.
- **Topic shape.** Topics live as `PROGMEM` strings near the top of the sketch. Reorganise as needed (e.g. add a site prefix `home/garden/TX07K_TXC/...`).
- **Subscriptions.** `MQTTMessageReceive()` is intentionally empty; add handlers there if you want the bridge to react to commands.

## Troubleshooting

- *No data ever published.* Confirm the RF receiver actually sees the sensor — the sketch dumps raw bytes plus `\r\n` to USB Serial at 57600 baud on every successful decode. If nothing arrives there, the issue is upstream of MQTT.
- *`CRC Failed` on Serial.* The library prints this when a frame is received but the checksum fails. Common causes: weak RF signal, antenna missing on receiver, sensor going low-battery.
- *Wifi status / Connect loops in Serial.* The ESP8266 isn't responding to AT commands — check baud rate (57600 on both `softwareSerial.begin` and the ESP firmware), wiring, and 3.3 V supply current.
- *`Low memory available` warning at compile.* Expected with the current feature set; ~338 B remains for stack. If you add more globals, prefer `PROGMEM`/`F()` literals before increasing buffer sizes.

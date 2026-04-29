#ifndef PACKET_PROTOCOL_H
#define PACKET_PROTOCOL_H

#include <devices.h>
#include <stddef.h>
#include <stdint.h>
/**
 * @file PacketProtocol.h
 * @brief Standardized communication buffer protocol.
 *
 * Design principles
 * ─────────────────
 * • PacketType  →  describes the PAYLOAD FORMAT (which struct to cast to).
 * • deviceID    →  first byte of every payload; identifies WHO sent the frame.
 *
 * This separation means adding a new room/node only requires a new deviceID
 * in devices.h — the parser and structs stay unchanged.
 *
 * Frame layout
 * ────────────
 *   [0]      Magic          0xAA
 *   [1]      Version        0x01
 *   [2]      Type           PacketType  ← HOW to read the payload
 *   [3..4]   PayloadLength  uint16_t, little-endian
 *   [5]      deviceID       first byte of payload  ← WHO sent it
 *   [6..N]   rest of payload
 *   [N+1]    XOR checksum   over bytes [0..N]
 *
 * Total frame size = HEADER_SIZE + payloadLength + 1
 *
 * Encoding conventions — ALL sensor fields are uint16_t
 * ───────────────────────────────────────────────────────
 *   temperature  →  (float + 50.0) × 128    range  -50 … +462 °C
 *   humidity     →   float         × 128    range    0 … 100 %
 *   pressure     →   float         × 16     range    0 … 4095 hPa
 *   battery      →   mV, 1:1                unsigned, no scaling
 *   blind pos    →   0-100 %                uint8_t, no scaling
 *
 * Encode (sender side, C)
 * ────────────────────────
 *   d.temperature = PP_ENCODE_TEMP(temp);   // (temp + 50) * 128
 *   d.humidity    = PP_ENCODE_HUM(hum);     // hum * 128
 *   d.pressure    = PP_ENCODE_PRESS(press); // press * 16
 *
 * Decode (receiver side, C)
 * ──────────────────────────
 *   float t = PP_DECODE_TEMP(d->temperature);  // val/128.0 - 50.0
 *   float h = PP_DECODE_HUM(d->humidity);      // val/128.0
 *   float p = PP_DECODE_PRESS(d->pressure);    // val/16.0
 *
 * Decode (Node-RED / JavaScript)
 * ────────────────────────────────
 *   temp:     payload.readUInt16LE(offset) / 128 - 50
 *   humidity: payload.readUInt16LE(offset) / 128
 *   pressure: payload.readUInt16LE(offset) / 16
 *   blind:    payload.readUInt8(offset)           // already 0-100
 */

// ── Protocol constants
// ────────────────────────────────────────────────────────
#define PACKET_MAGIC 0xAA
#define PACKET_VERSION 0x04 // dopo aggiornamento ESP NOW broadcat
#define HEADER_SIZE 5       // bytes before payload
#define PACKET_MIN_SIZE 6   // smallest legal frame

// ── Encoding helpers
// ──────────────────────────────────────────────────────────
#define PP_TEMP_OFFSET 50.0f
#define PP_TEMP_SCALE 128.0f
#define PP_HUM_SCALE 128.0f
#define PP_PRESS_SCALE 16.0f

#define PP_ENCODE_TEMP(t) ((uint16_t)(((t) + PP_TEMP_OFFSET) * PP_TEMP_SCALE))
#define PP_DECODE_TEMP(v) ((v) / PP_TEMP_SCALE - PP_TEMP_OFFSET)
#define PP_ENCODE_HUM(h) ((uint16_t)((h) * PP_HUM_SCALE))
#define PP_DECODE_HUM(v) ((v) / PP_HUM_SCALE)
#define PP_ENCODE_PRESS(p) ((uint16_t)((p) * PP_PRESS_SCALE))
#define PP_DECODE_PRESS(v) ((v) / PP_PRESS_SCALE)

// ── Packet types (= payload format) ──────────────────────────────────────────
typedef enum {
  TYPE_ACK = 0x00,      // Binary ACK (deviceID + status)
  TYPE_ANNOUNCE = 0x01, // device announcement/handshake (v2+)
  TYPE_COMMAND = 0x02,  // command frame
  TYPE_METEO = 0x03,    // Stazione meteo outdoor (16 byte) → struct meteoData
  TYPE_DHT = 0x04,      // Sensori indoor (6 byte) → struct dhtData o ds18Data
  TYPE_DS18 = 0x04,     // Alias per compatibilità, stesso formato di TYPE_DHT
  TYPE_BME = 0x05,      // BME280 (9 byte) → struct bmeData
  TYPE_PZEM = 0x06,     // PZEM energy meter (9 byte) → struct EneMainData
  TYPE_TENDE = 0x07,    // Tende Tuya (6 byte) → struct tendeData
  TYPE_TENDE_COMMAND =
      0x0C,              // Tende Tuya Command (4 byte) → struct tendeCmdData
  TYPE_TIME = 0x08,      // Time and date (4 byte) → struct timeData
  TYPE_BOILER = 0x09,    // Caldaia (6 byte) → struct boilerData
  TYPE_CAMINETTO = 0x0A, // Caminetto (6 byte) → struct caminettoData
  TYPE_PID_CONFIG =
      0x0B, // PID setup parameters (22 byte) → struct pidConfigData
  TYPE_UNKNOWN = 0xFF
} PacketType;

// ── Payload structures
// ────────────────────────────────────────────────────────

#pragma pack(push, 1)
/**
 * TYPE_ACK — Universal binary confirmation
 * Size: 1 + 1 = 2 bytes
 */
struct ackData {
  uint8_t deviceID; // The target/actuator of this ACK (es: 0x04)
  uint8_t
      status; // AckState (1=OK, 2=END, 3=FAILED, 4=ERROR, 5=SWITCH_TRANSPORT)
  uint8_t cmdEcho; // The command being confirmed (es: CMD_POWER_ON)
  uint8_t valEcho; // The resulting value / state (0/1)
};

// AckState constants
#define AC_OK 0x01
#define AC_END 0x02
#define AC_FAILED 0x03
#define AC_ERROR 0x04
#define AC_SWITCH_TO_ESPNOW 0x05
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * TYPE_ANNOUNCE — Handshake/Intro packet (broadcast or via bridge)
 * Size: 1 + 1 + 2 = 4 bytes
 */
struct announceData {
  uint8_t deviceID;     // vedi devices.h
  uint8_t protoVersion; // PACKET_VERSION (0x02)
  uint16_t fwVersion;   // firmware version (macro 'versione')
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * TYPE_CAMINETTO — Specialized packet for fireplace control
 * Size: 1 + 2 + 2 + 1 = 6 bytes
 */
struct caminettoData {
  uint8_t deviceID; // DEV_CAMINETTO_DS18B20 (0x50)
  uint16_t tempAmb; // Ambiente/Circuito (PP_ENCODE_TEMP)
  uint16_t tempK;   // Camino/Sonda K (PP_ENCODE_TEMP)
  uint8_t fanPid;   // PWM output (0-255)
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * TYPE_PID_CONFIG — PID and configuration parameters for fan control
 * Size: 1 + 1 + 1 + 4*5 = 23 bytes
 */
struct pidConfigData {
  uint8_t deviceID;   // DEV_CAMINETTO, ...
  uint8_t updateMask; // BITS: 0=sTemp, 1=sMinFan, 2=sMaxFan, 3=P, 4=I, 5=D,
                      // 7(0x80)=CMD_FROM_MASTER
  uint8_t sTemp;      // Target temperature
  float sMinFan;      // Min PWM
  float sMaxFan;      // Max PWM
  float P;            // Proportional gain
  float I;            // Integral gain
  float D;            // Derivative gain
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * @brief Structure for weather data telemetry.
 * Total size: 16 bytes.
 */
struct meteoData {
  uint8_t deviceID;         // Unique device identifier
  int16_t humidityBMP;      // Humidity (float * 128)
  int16_t temperatureBMP;   // Temperature (float * 128)
  int16_t externalPressure; // Atmospheric pressure (float * 128)
  uint16_t battery;         // Battery voltage in mV
  uint16_t moisture;        // Soil moisture raw ADC value
  uint8_t padding[3];       // 3-byte padding to maintain 16-byte total size
  uint8_t counter;          // Record sequence counter
  uint8_t checksum;         // Integrity check byte
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * TYPE_DHT — DHT22 room sensor (salotto, camera, …)
 * Size: 1 + 2 + 2 + 1 = 6 bytes
 */
struct dhtData {
  uint8_t deviceID;     // vedi file devices.h
  uint16_t humidity;    // PP_ENCODE_HUM(float)
  uint16_t temperature; // PP_ENCODE_TEMP(float)
  uint8_t comfort; // bitmask: bit0=OK 1=TooHot 2=TooCold 3=TooDry 4=TooHumid
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * TYPE_DS18 — DS18B20 temperature-only sensor (bagno, …)
 * humidity is always 0 — kept for layout symmetry with dhtData.
 * Size: 1 + 2 + 2 + 1 = 6 bytes
 */
struct ds18Data {
  uint8_t deviceID;     // DEV_BAGNO, …
  uint16_t humidity;    // always 0 (no humidity sensor)
  uint16_t temperature; // PP_ENCODE_TEMP(float)
  uint8_t comfort;      // computed from temperature only
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * TYPE_BME — BME280 weather station (Mariner, …)
 * Size: 1 + 2 + 2 + 2 + 2 = 9 bytes
 */
struct bmeData {
  uint8_t deviceID;     // DEV_MARINER, …
  uint16_t humidity;    // PP_ENCODE_HUM(float)
  uint16_t temperature; // PP_ENCODE_TEMP(float)
  uint16_t pressure;    // PP_ENCODE_PRESS(float)   hPa
  uint16_t battery;     // mV, raw (1:1, no scaling)
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * TYPE_PZEM — energy meter
 * Size: 1 + 2 + 2 + 2 + 2 = 9 bytes
 */
struct EneMainData {
  uint8_t deviceID; // DEV_PZEM, …
  uint16_t v;       // voltage  float × 16
  uint16_t i;       // current  float × 128
  uint16_t c;       // cos φ    float × 128
  uint16_t e;       // power    W (raw)
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * TYPE_TENDE — Tuya blind positions (Node-RED → subscribers)
 * Size: 1 + 5 = 6 bytes
 */
struct tendeData {
  uint8_t deviceID; // DEV_TUYA_TENDE
  uint8_t pos[5];   // pos[i] = blind i opening, 0-100 %
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * TYPE_TENDE_COMMAND — Request blind movement (Chrono -> Bridge)
 * Size: 1 + 1 + 1 + 1 = 4 bytes
 */
struct tendeCmdData {
  uint8_t deviceID;   // CTRL_TUYA_TENDE (0x05)
  uint8_t blindIndex; // 0-4
  uint8_t command;    // T_OPEN=16, T_CLOSE=17, T_STOP=15, T_POS=3
  uint8_t val;        // percentage (if command == T_POS), otherwise 0
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * TYPE_TIME — System time synchronization
 * Size: 1 + 1 + 1 + 1 = 4 bytes
 */
struct timeData {
  uint8_t deviceID; // Who sent the time (usually CTRL_MAIN_CONTROL)
  uint8_t hour;     // 0-23
  uint8_t minute;   // 0-59
  uint8_t day;      // 1=Lun, 2=Mar, 3=Mer, 4=Gio, 5=Ven, 6=Sab, 7=Dom
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * TYPE_BOILER — Boiler status (temperature + valve opening)
 * Size: 1 + 2 + 2 = 5 bytes (allineato a 6 per sicurezza?)
 */
struct boilerData {
  uint8_t deviceID;     // DEV_CALDAIA_DS18B20
  uint16_t temperature; // PP_ENCODE_TEMP(float)
  uint16_t valvePos;    // raw analogRead (0-1023)
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * TYPE_COMMAND — General command packet (for relays, etc.)
 * Size: 1 + 1 + 1 = 3 bytes
 */
struct cmdData {
  uint8_t deviceID; // Target device ID
  uint8_t command;  // Command type (CommandID)
  uint8_t value;    // 0 = Off, 1 = On, or generic value
};
#pragma pack(pop)

typedef enum {
  CMD_POWER_OFF = 0x00,
  CMD_POWER_ON = 0x01,
  CMD_SYS_RESET = 0xEE,
  CMD_SYS_SLEEP =
      0xEF, // Put device into deep sleep; value = seconds (0 → default)
  CMD_SYS_UPDATE = 0xF0
} CommandID;

// ── Frame header
// ──────────────────────────────────────────────────────────────
#pragma pack(push, 1)
/** Fixed 5-byte header preceding every payload. */
struct StandardHeader {
  uint8_t magic;          // PACKET_MAGIC   (0xAA)
  uint8_t version;        // PACKET_VERSION (0x03)
  uint8_t type;           // PacketType  ← HOW
  uint16_t payloadLength; // little-endian
};
#pragma pack(pop)

// ── Parsed-packet result
// ──────────────────────────────────────────────────────
/**
 * @brief Output of pp_parsePacket().
 *
 * On success:
 *   header   — decoded 5-byte header.
 *   payload  — pointer into the caller's buffer (zero-copy; do not free).
 *
 * Usage:
 *   ParsedPacket pkt;
 *   if (pp_parsePacket(buf, len, &pkt) == 0) {
 *       switch (pkt.header.type) {
 *
 *           case TYPE_DHT: {
 *               const dhtData *d = (const dhtData *)pkt.payload;
 *               float t = PP_DECODE_TEMP(d->temperature);
 *               float h = PP_DECODE_HUM(d->humidity);
 *               break;
 *           }
 *           case TYPE_DS18: {
 *               const ds18Data *d = (const ds18Data *)pkt.payload;
 *               float t = PP_DECODE_TEMP(d->temperature);
 *               break;
 *           }
 *           case TYPE_BME: {
 *               const bmeData *d = (const bmeData *)pkt.payload;
 *               float t = PP_DECODE_TEMP(d->temperature);
 *               float p = PP_DECODE_PRESS(d->pressure);
 *               break;
 *           }
 *           case TYPE_TENDE: {
 *               const tendeData *d = (const tendeData *)pkt.payload;
 *               uint8_t n = pkt.header.payloadLength - 1;
 *               for (int i = 0; i < n; i++) {
 *                   // d->pos[i] = position of blind i (0-100%)
 *               }
 *               break;
 *           }
 *       }
 *   }
 */
typedef struct {
  StandardHeader header;
  const uint8_t *payload; // points into original buffer — do not free
} ParsedPacket;

// ── Public API
// ────────────────────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

/** XOR over every byte in data[0..length-1]. */
uint8_t pp_calculateXOR(const uint8_t *data, size_t length);

/**
 * Build a complete frame into outBuffer.
 * outBuffer must be at least (payloadLen + 6) bytes.
 * Returns total frame size, or 0 on error.
 */
size_t pp_buildPacket(uint8_t type, const uint8_t *payload, uint16_t payloadLen,
                      uint8_t *outBuffer);

/**
 * @deprecated Use mqttWifi::sendBinaryCommandWithAck instead.
 * This remains for legacy binary support but lacks robust ACK handling.
 */
//void pp_sendLegacyCommand(uint8_t deviceID, uint8_t command, uint8_t value);

/**
 * Validate a received frame (magic + size + XOR checksum).
 * Returns  0  if valid.
 *         -1  NULL pointer or frame too small.
 *         -2  bad magic word.
 *         -3  bufferSize does not match declared payload length.
 *         -4  XOR checksum mismatch.
 */
int pp_validatePacket(const uint8_t *buffer, size_t bufferSize);

/**
 * Validate + decode a received frame into *out.
 * out->payload points into buffer (zero-copy).
 * Returns same codes as pp_validatePacket(), plus:
 *         -5  out is NULL.
 */
int pp_parsePacket(const uint8_t *buffer, size_t bufferSize, ParsedPacket *out);

#ifdef __cplusplus
}
#endif

#endif // PACKET_PROTOCOL_H
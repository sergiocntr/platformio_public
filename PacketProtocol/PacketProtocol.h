#ifndef PACKET_PROTOCOL_H
#define PACKET_PROTOCOL_H

#include <devices.h>
#include <stddef.h>
#include <stdint.h>

// ── Protocol constants
#define PACKET_MAGIC 0xAA
#define PACKET_VERSION 0x05 // Aggiunto uptime a boiler, pzem e node report
#define HEADER_SIZE 5       // bytes before payload
#define PACKET_MIN_SIZE 6   // smallest legal frame

// ── Encoding helpers
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

// ── Packet types
typedef enum {
  TYPE_ACK = 0x00,
  TYPE_ANNOUNCE = 0x01,
  TYPE_COMMAND = 0x02,
  TYPE_METEO = 0x03,
  TYPE_DHT = 0x04,
  TYPE_DS18 = 0x04,
  TYPE_BME = 0x05,
  TYPE_PZEM = 0x06,
  TYPE_TENDE = 0x07,
  TYPE_TIME = 0x08,
  TYPE_BOILER = 0x09,
  TYPE_CAMINETTO = 0x0A,
  TYPE_PID_CONFIG = 0x0B,
  TYPE_TENDE_COMMAND = 0x0C,
  TYPE_NODE_REPORT = 0x0D,
  TYPE_UNKNOWN = 0xFF
} PacketType;

#pragma pack(push, 1)
struct ackData {
  uint8_t deviceID;
  uint8_t status;
  uint8_t cmdEcho;
  uint8_t valEcho;
};

#define AC_OK 0x01
#define AC_END 0x02
#define AC_FAILED 0x03
#define AC_ERROR 0x04
#define AC_SWITCH_TO_ESPNOW 0x05

struct announceData {
  uint8_t deviceID;
  uint8_t protoVersion;
  uint16_t fwVersion;
};

struct caminettoData {
  uint8_t deviceID;
  uint16_t tempAmb;
  uint16_t tempK;
  uint8_t fanPid;
};

struct pidConfigData {
  uint8_t deviceID;
  uint8_t updateMask;
  uint8_t sTemp;
  float sMinFan;
  float sMaxFan;
  float P;
  float I;
  float D;
};

struct meteoData {
  uint8_t deviceID;
  int16_t humidityBMP;
  int16_t temperatureBMP;
  int16_t externalPressure;
  uint16_t battery;
  uint16_t moisture;
  uint8_t padding[3];
  uint8_t counter;
  uint8_t checksum;
};

struct dhtData {
  uint8_t deviceID;
  uint16_t humidity;
  uint16_t temperature;
  uint8_t comfort;
  uint32_t uptime;      // v5.0
};

struct ds18Data {
  uint8_t deviceID;
  uint16_t humidity;
  uint16_t temperature;
  uint8_t comfort;
  uint32_t uptime;      // v5.0
};

struct bmeData {
  uint8_t deviceID;
  uint16_t humidity;
  uint16_t temperature;
  uint16_t pressure;
  uint16_t battery;
};

struct EneMainData {
  uint8_t deviceID;
  uint16_t v;
  uint16_t i;
  uint16_t c;
  uint16_t e;
  uint32_t uptime;      // v5.0
};

struct tendeData {
  uint8_t deviceID;
  uint8_t pos[5];
};

struct tendeCmdData {
  uint8_t deviceID;
  uint8_t blindIndex;
  uint8_t command;
  uint8_t val;
};

struct timeData {
  uint8_t deviceID;
  uint8_t hour;
  uint8_t minute;
  uint8_t day;
};

struct boilerData {
  uint8_t deviceID;
  uint16_t temperature;
  uint16_t valvePos;
  uint32_t uptime;      // v5.0
};

struct cmdData {
  uint8_t deviceID;
  uint8_t command;
  uint8_t value;
};

struct nodeEntry {
  uint8_t deviceID;
  uint8_t nodeType;
  uint16_t fwVersion;
  uint32_t lastSeenSec;
  uint32_t uptimeSec;
  uint8_t mac[6];
  uint8_t txFailCount;
  uint8_t rssi;
};

struct nodeReportData {
  uint8_t deviceID;
  uint8_t nodeCount;
};

struct nodeEntryCompact {
  uint8_t deviceID;
  uint8_t flags;
  uint16_t fwVersion;
  uint32_t lastSeenSec;
  uint32_t uptimeSec;   // v5.0
  uint8_t mac[6];
  uint8_t txFailCount;
  uint8_t rssi;
}; 

typedef enum {
  CMD_POWER_OFF = 0x00,
  CMD_POWER_ON = 0x01,
  CMD_SYS_RESET = 0xEE,
  CMD_SYS_SLEEP = 0xEF,
  CMD_SYS_UPDATE = 0xF0
} CommandID;

struct StandardHeader {
  uint8_t magic;
  uint8_t version;
  uint8_t type;
  uint16_t payloadLength;
};
#pragma pack(pop)

typedef struct {
  StandardHeader header;
  const uint8_t *payload;
} ParsedPacket;

#ifdef __cplusplus
extern "C" {
#endif

uint8_t pp_calculateXOR(const uint8_t *data, size_t length);
size_t pp_buildPacket(uint8_t type, const uint8_t *payload, uint16_t payloadLen, uint8_t *outBuffer);
int pp_validatePacket(const uint8_t *buffer, size_t bufferSize);
int pp_parsePacket(const uint8_t *buffer, size_t bufferSize, ParsedPacket *out);

#ifdef __cplusplus
}
#endif

#endif // PACKET_PROTOCOL_H
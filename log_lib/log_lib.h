#ifndef LOG_LIB_H
#define LOG_LIB_H

#include <Arduino.h>

#ifdef ESP8266_BUILD
#include <ESP8266WiFi.h>
#elif ESP32_BUILD
#include <WiFi.h>
#endif
#include <WiFiUdp.h>

// ============================================================
//  Configurazione Default (sovrascivibile da platformio.ini)
// ============================================================

#ifndef DEBUG_LEVEL
  #define DEBUG_LEVEL 1
#endif

#ifndef UDP_LOG_IP
  #define UDP_LOG_IP "192.168.1.100"
#endif

#ifndef UDP_LOG_PORT
  #define UDP_LOG_PORT 4444
#endif

#ifndef DISABLE_UDP_LOG
  #define DEBUG_UDP_LOG
#endif

// Selezione Porta Seriale per Log
#ifndef LOG_SERIAL_PORT
  #define LOG_SERIAL_PORT Serial
#endif

// Abilitazione Serial Log
// Se USE_NEXTION o DISABLE_SERIAL_LOG sono definiti, la seriale di log viene disabilitata 
// a meno che non sia esplicitamente forzata con FORCE_SERIAL_LOG.
#if defined(USE_NEXTION) || defined(DISABLE_SERIAL_LOG)
  #ifdef FORCE_SERIAL_LOG
    #ifndef DEBUG_SERIAL_LOG
      #define DEBUG_SERIAL_LOG
    #endif
  #else
    #ifdef DEBUG_SERIAL_LOG
      #undef DEBUG_SERIAL_LOG
    #endif
  #endif
#else
  #ifndef DEBUG_SERIAL_LOG
    #define DEBUG_SERIAL_LOG
  #endif
#endif

// ============================================================
//  API Pubblica
// ============================================================

#ifdef DEBUG_UDP_LOG
extern WiFiUDP udpLog;
extern char log_device_name[32];
void udpLogBegin();
void logSetDeviceName(const char* name);
void udpLogSend(const char *msg);
void udpLogSend_f(const char *fmt, ...);

class UdpLogger {
public:
    UdpLogger() : _port(0), _enabled(true) {
        _deviceName[0] = '\0';
    }
    void begin(const char* ip, uint16_t port) {
        _ip.fromString(ip);
        _port = port;
        _udp.begin(2222 + random(1000));
    }
    void setDeviceName(const char* name) {
        if (name) strncpy(_deviceName, name, sizeof(_deviceName) - 1);
    }
    void setEnabled(bool enabled) { _enabled = enabled; }
    void send(const char* msg) {
        if (!_enabled || WiFi.status() != WL_CONNECTED) return;
        _udp.beginPacket(_ip, _port);
        if (_deviceName[0] != '\0') {
            _udp.print("[");
            _udp.print(_deviceName);
            _udp.print("] ");
        }
        _udp.write((const uint8_t*)msg, strlen(msg));
        _udp.endPacket();
    }
    void sendf(const char* fmt, ...) {
        if (!_enabled) return;
        char buf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        send(buf);
    }
private:
    WiFiUDP _udp;
    IPAddress _ip;
    uint16_t _port;
    bool _enabled;
    char _deviceName[32];
};
#endif

void logSerialBegin(uint32_t baud = 38400);

template <typename T> inline String _toStr(T val) { return String(val); }
inline String _toStr(IPAddress ip) { return ip.toString(); }

#ifdef DEBUG_SERIAL_LOG
  #if defined(USE_NEXTION) && !defined(FORCE_SERIAL_LOG)
    #define _SERIAL_LOG(prefix, fmt, ...)
  #else
    #define _SERIAL_LOG(prefix, fmt, ...) LOG_SERIAL_PORT.printf(prefix fmt "\n", ##__VA_ARGS__)
  #endif
#else
  #define _SERIAL_LOG(prefix, fmt, ...)
#endif

#ifdef DEBUG_UDP_LOG
  #define _UDP_LOG(fmt, ...) udpLogSend_f(fmt, ##__VA_ARGS__)
#else
  #define _UDP_LOG(fmt, ...)
#endif

// ============================================================
//  Macro di LOG
// ============================================================

#define LOG_ERROR(fmt, ...) \
    do { \
        _UDP_LOG("[ERR] " fmt, ##__VA_ARGS__); \
        _SERIAL_LOG("[ERR] ", fmt, ##__VA_ARGS__); \
    } while (0)

#if DEBUG_LEVEL >= 1
  #define LOG_WARN(fmt, ...) \
      do { \
          _UDP_LOG("[WRN] " fmt, ##__VA_ARGS__); \
          _SERIAL_LOG("[WRN] ", fmt, ##__VA_ARGS__); \
      } while (0)
#else
  #define LOG_WARN(...) do {} while (0)
#endif

#if DEBUG_LEVEL >= 2
  #define LOG_INFO(fmt, ...) \
      do { \
          _UDP_LOG("[INF] " fmt, ##__VA_ARGS__); \
          _SERIAL_LOG("[INF] ", fmt, ##__VA_ARGS__); \
      } while (0)
#else
  #define LOG_INFO(...) do {} while (0)
#endif

#if DEBUG_LEVEL >= 3
  #define LOG_VERBOSE(fmt, ...) \
      do { \
          _UDP_LOG("[VRB] " fmt, ##__VA_ARGS__); \
          _SERIAL_LOG("[VRB] ", fmt, ##__VA_ARGS__); \
      } while (0)
#else
  #define LOG_VERBOSE(...) do {} while (0)
#endif

enum MotivoSpegnimento {
  PUBLISH_FALLITO = 0,
  COMANDO_SYSTEM_TOPIC = 1,
  WIFI_TIMEOUT_CONNESSIONE = 2,
  MQTT_TIMEOUT_CONNESSIONE = 3,
  WIFI_FALLITO_SETUP = 4,
  MQTT_FALLITO_RISVEGLIO = 5,
  WIFI_FALLITO_RISVEGLIO = 6,
  NEXTION_SETUP_FAILED = 7,
  DHT_SETUP_FAILED = 8,
  ONLY_DISCONNETS = 9,
  SETUP_OK = 252,
  CONN_OK = 253,
  SHUTDOWN_FROM_MQTT = 254,
  CLEAN_SHUTDOWN = 255
};
extern MotivoSpegnimento m_wifi_status;

enum RelayIdx {
  RISCALDAMENTO = 0, ACQUA = 1, ALLARME = 2, CAMERA = 3, TERRAZZA = 4, CALDAIA = 5, ENERGIA = 6, MAX_RELAY = 7
};

#endif

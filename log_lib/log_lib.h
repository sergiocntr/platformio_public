#ifndef ENERGY_LOG_H
#define ENERGY_LOG_H

#include <Arduino.h>

#ifdef ESP8266_BUILD
#include <ESP8266WiFi.h>
#elif ESP32_BUILD
#include <WiFi.h>
#endif

#include <WiFiUdp.h>

// ============================================================
//  Configurazione Debug e Logger
// ============================================================
#define DEBUG_CHRONO
#define DEBUG_UDP_LOG // UDP sempre attivo
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 3 // 0=solo errori, 1=warning, 2=info, 3=verbose
#endif

#ifndef DEBUG_UDP_LOG
#define DEBUG_UDP_LOG
#endif

#define UDP_LOG_IP "192.168.1.100" // IP del PC che riceve i log
#define UDP_LOG_PORT 4444

#ifdef DEBUG_UDP_LOG
extern WiFiUDP udpLog;

void udpLogBegin();
void udpLogSend(const char *msg);
void udpLogSend_f(const char *fmt, ...);

// Helper per convertire qualsiasi tipo, incluso IPAddress
template <typename T> inline String _toStr(T val) { return String(val); }
inline String _toStr(IPAddress ip) { return ip.toString(); }

// Macro helper per gestire la seriale: se Nextion è attivo, non scriviamo sulla seriale
#if defined(USE_NEXTION) || defined(DISABLE_SERIAL_LOG)
  #define _SERIAL_LOG(prefix, fmt, ...) // Nessun output seriale
#else
  #define _SERIAL_LOG(prefix, fmt, ...) if (Serial) { Serial.printf(prefix fmt "\n", ##__VA_ARGS__); }
#endif

#define LOG_ERROR(fmt, ...)                                                    \
  do {                                                                         \
    udpLogSend_f(fmt, ##__VA_ARGS__);                                          \
    _SERIAL_LOG("[ERR] ", fmt, ##__VA_ARGS__)                                 \
  } while (0)

#if DEBUG_LEVEL >= 1
#define LOG_WARN(fmt, ...)                                                     \
  do {                                                                         \
    udpLogSend_f(fmt, ##__VA_ARGS__);                                          \
    _SERIAL_LOG("[WRN] ", fmt, ##__VA_ARGS__)                                 \
  } while (0)
#else
#define LOG_WARN(...) do {} while (0)
#endif

#if DEBUG_LEVEL >= 2
#define LOG_INFO(fmt, ...)                                                     \
  do {                                                                         \
    udpLogSend_f(fmt, ##__VA_ARGS__);                                          \
    _SERIAL_LOG("[INF] ", fmt, ##__VA_ARGS__)                                 \
  } while (0)
#else
#define LOG_INFO(...) do {} while (0)
#endif

#if DEBUG_LEVEL >= 3
#define LOG_VERBOSE(fmt, ...)                                                  \
  do {                                                                         \
    udpLogSend_f(fmt, ##__VA_ARGS__);                                          \
    _SERIAL_LOG("[VRB] ", fmt, ##__VA_ARGS__)                                 \
  } while (0)
#else
#define LOG_VERBOSE(...) do {} while (0)
#endif

#elif defined(DEBUG_CHRONO)
// Seriale classica (solo se UDP è spento)
#define logSerial Serial
#define logSerialBegin(a) logSerial.begin(a)
#define logSerialPrint(a) logSerial.print(a)
#if defined(USE_NEXTION) || defined(DISABLE_SERIAL_LOG)
  #define LOG_ERROR(...) do {} while (0)
  #define LOG_WARN(...) do {} while (0)
  #define LOG_INFO(...) do {} while (0)
  #define LOG_VERBOSE(...) do {} while (0)
#else
  #define LOG_ERROR(...) logSerial.printf(__VA_ARGS__)
  #define LOG_WARN(...) logSerial.printf(__VA_ARGS__)
  #define LOG_INFO(...) logSerial.printf(__VA_ARGS__)
  #define LOG_VERBOSE(...) logSerial.printf(__VA_ARGS__)
#endif

#endif

// ========== ENUM MOTIVI SPEGNIMENTO ==========
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
  ONLY_DISCONNETS = 9, // usato per sonda meteo e test, non va in deep sleep
  SETUP_OK = 252,
  CONN_OK = 253,
  SHUTDOWN_FROM_MQTT = 254,
  CLEAN_SHUTDOWN = 255
};
extern MotivoSpegnimento m_wifi_status;
enum RelayIdx {
  RISCALDAMENTO = 0,
  ACQUA = 1,
  ALLARME = 2,
  CAMERA = 3,
  TERRAZZA = 4,
  CALDAIA = 5,
  ENERGIA = 6,
  MAX_RELAY = 7
};
#endif // ENERGY_LOG_H

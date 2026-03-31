#pragma once
#include <Arduino.h>
#include <log_lib.h>
#ifdef ESP32_MQTT
// ========== SELEZIONE TRASPORTO (GLOBAL) ==========
// Rendi uno dei due attivo per tutta la rete:
#define USE_MQTT_WIFI // Standard: WiFi + MQTT (Broker TCP)

#else
#define USE_MQTT_ESPNOW // Bridge: ESP-NOW -> ESP32 Gateway -> MQTT
#endif
// Canale WiFi fisso per ESP-NOW (deve coincidere col router)
#define WIFI_CHANNEL_GATEWAY 12

// MAC Address dell'ESP32 Gateway (da aggiornare dopo il primo boot del Gateway)
const uint8_t ESPNOW_GATEWAY_MAC[6] = {0x58, 0xbf, 0x25, 0x36, 0xd2, 0xb0};

enum SensIdx {
  INT = 0, // Interno (Chrono)
  EXT = 1, // Esterno (ESPmeteo)
  BAG = 2, // Bagno
  MAX_SENS = 4
};
enum Tende {
  p_l, // 0 pl_cr id 10 - pl_bar id 5
  t_l, // 1 tl_cr id 11 - tl_bar id 6
  p_s, // 2 ps_cr id 12 - ps_bar id 7
  t_s, // 3 ts_cr id 13 - ts_bar id 8
  p_c  // 4 pc_cr id 14 - pc_bar id 9
};
constexpr int NUM_TENDE = 5;
// Enum per i comandi delle tende
enum ComandoTende {

  T_STOP = 15,  // st_cr id 15
  T_OPEN = 16,  // up_cr id 16
  T_CLOSE = 17, // dw_cr id 17
};
extern ComandoTende comandoTenda;

struct __attribute__((packed)) SystemState {
  float temps[MAX_SENS];
  float hums[MAX_SENS];
  float waterTemp;
  uint16_t powerW;
  uint8_t pos[6]; // slider tende
  bool relays[MAX_RELAY];
  uint8_t currPage;
  uint8_t selectionMask; // Bitmask per selezione multipla tende
  char timeStr[8];       // Esempio: "HH:MM"
  char dayStr[8];        // Esempio: "Lunedi"
};

extern SystemState stato;

#ifndef SYSTEM_PROFILES_H
#define SYSTEM_PROFILES_H

/**
 * @brief Resilient Star - Bitmask System Configuration v2.1
 *
 * Usa -DSYS_CONFIG=0b<bits> nel platformio.ini
 *
 * Mappa dei Bit (LEGENDA VISIVA):
 * ---------------------------------------------------------------------------
 * BIT | Valore Bin  | Funzione     | Descrizione
 * ---------------------------------------------------------------------------
 * [0] | 0b000000001 | LOG_SERIAL   | Abilita Serial Log
 * [1] | 0b000000010 | LOG_UDP      | Abilita UDP Log (richiede WiFi)
 * [2] | 0b000000100 | LOG_FORCE    | Forza Serial Log anche con conflitti HW
 *     |             |              |
 * [4] | 0b000010000 | RAD_FORCE_NOW| Tier 1.5: Exclusive ESP-NOW (No WiFi
 * fallback) [5] | 0b000100000 | RAD_GATEWAY  | Modalità Gateway (ESP32 Bridge)
 *     |             |              |
 * [8] | 0b100000000 | HW_NEXTION   | Presente Nextion (Inibisce Serial Log)
 * [8] | 0b100000000 | HW_PZEM      | Presente PZEM (Inibisce Serial Log)
 * ---------------------------------------------------------------------------
 *
 * ESEMPI PRATICI:
 * - ESPmeteo (Radio Exclusive + Serial Log):
 *   SYS_CONFIG = 0b000010001 (Radio bit 4 + Serial bit 0)
 *
 * - Chrono (Nextion + UDP Log):
 *   SYS_CONFIG = 0b100000010 (Nextion bit 8 + UDP bit 1)
 */

#ifndef SYS_CONFIG
#define SYS_CONFIG 0b000000011 // Default: Serial + UDP Log, Radio Standard
#endif

// --- LOGICA DI ESTRAZIONE BIT ---

// 1. Radio
#if (SYS_CONFIG & 0b000010000)
#define FORCE_ESPNOW
#endif

#if (SYS_CONFIG & 0b000100000)
#define ESP32_MQTT
#endif

#ifndef ESP32_MQTT
#define USE_MQTT_ESPNOW
#endif

// 2. Hardware & Conflitti
#if (SYS_CONFIG & 0b100000000)
#define USE_NEXTION
#endif

#if (SYS_CONFIG & 0b1000000000)
#define USE_PZEM
#endif

// 3. Logica Log Seriale (con protezione conflitti)
#if (SYS_CONFIG & 0b000000001)
#if defined(USE_NEXTION) || defined(USE_PZEM)
#if (SYS_CONFIG & 0b000000100)
#define DEBUG_SERIAL_LOG // Forzato
#warning                                                                       \
    "ATTENZIONE: Log Seriale FORZATO su porta condivisa con Nextion/PZEM (Serial Test Mode)"
#else
// Inibito automaticamente per conflitto hardware
#endif

#else
#define DEBUG_SERIAL_LOG // Abilitato normalmente
#endif
#endif

// 4. Logica Log UDP
#if (SYS_CONFIG & 0b000000010)
#define DEBUG_UDP_LOG
#else
#define DISABLE_UDP_LOG
#endif

// 5. Livelli di Debug (dedotti)
#ifndef DEBUG_LEVEL
#if (SYS_CONFIG & 0b000010000)
#define DEBUG_LEVEL 3 // Più debug per nodi a batteria
#else
#define DEBUG_LEVEL 1 // Standard
#endif
#endif

#endif // SYSTEM_PROFILES_H

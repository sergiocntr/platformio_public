#pragma once
#ifdef ESP8266_BUILD
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#define httpUpdate ESPhttpUpdate

#elif ESP32_BUILD
#include "esp_bt.h"
//#include "esp_bt_main.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#endif

#include "PacketProtocol.h"
#include "mqttWifi_transport.h"
#include <PubSubClient.h>
#include <log_lib.h>
#include <myIP.h>
#include <password.h>
#include <topic.h>
// #include "mqttWifiMessages.h"

namespace mqttWifi {
namespace SystemNetwork = mqttWifi; // Alias per transizione futura
extern PubSubClient client;

// ========== TOPIC DI SISTEMA (DEFAULT) ==========
// Questi topic sono sempre sottoscritti per garantire la resilienza
extern const char* CORE_TOPICS[];

// ========== STATI DI CONFERMA (ACK) ==========
enum AckState {
  NO_ACK = 0,
  ACK = 1,             // Conferma ricezione intermedia
  END = 2,             // Conferma finale (sessione completata)
  FAILED = 3,          // Errore checksum, riprovare
  ERROR = 4,           // Errore critico (richiesta reset)
  SWITCH_TRANSPORT = 5 // Forza il passaggio a ESP-NOW
};

extern volatile AckState ackStatus;
extern bool g_gateway_mac_trovato; // Stato di aggancio del gateway
extern bool g_gateway_paired;      // Handshake completato con successo
extern uint8_t g_real_gateway_mac[6]; // MAC address reale del gateway
extern uint32_t lastTimeSynced;    // Ultimo timestamp pacchetto TIME ricevuto
extern const uint32_t SYNC_TIMEOUT; // Timeout per switch (es. 5 min)

// trasporto selezionabile
void setNetworkTransport(NetworkTransportType t);
NetworkTransportType getNetworkTransport();

// ========== VARIABILI DI STATO ==========
extern IPAddress m_ip;
extern char m_mqtt_id[20];
extern uint8_t m_deviceID;
// Forward declarations
void adessoDormo(uint8_t mode, MotivoSpegnimento motivo);
bool sottoscriviTopic();

// ========== FUNZIONE LOG MOTIVO ==========
void logMotivoSpegnimento(MotivoSpegnimento motivo);

// ========== GESTIONE MODALITÀ RIPOSO ==========
void adessoDormo(uint8_t mode, MotivoSpegnimento motivo);

// ========== SETUP INIZIALE ==========
void setupWifi();

// ========== CONNESSIONE WIFI ==========
bool connectWifi();
// ========== CONNESSIONE MQTT ==========
bool connectMqtt();

// ========== SOTTOSCRIZIONE TOPIC ==========
// Sottoscrive i core topics + quelli opzionali del progetto
bool sottoscriviTopics(const char *progetto_topics[] = nullptr);

// ========== AGGIORNAMENTO FIRMWARE ==========
void checkForUpdates();

// ========== GESTIONE PUBBLICAZIONE & ACK ==========
bool publish(const char *topic, const char *message, bool retained = false);
bool publish(const char *topic, const uint8_t *payload, size_t length,
             bool retained = false);

// Invia un pacchetto ed aspetta la conferma (timeout standard 2s)
AckState publishWithAck(const char *topic, const uint8_t *payload,
                        size_t length, uint8_t retries = 1, uint32_t timeoutMs = 2000);

// Aspetta un ACK via ESP-NOW o MQTT (usa il loop client internally)
AckState waitForAck(uint32_t timeoutMs = 2000);

// Forza lo stato di ACK (chiamata solitamente dalla callback del progetto)
void setAckStatus(AckState s);

// ========== GESTIONE PRINCIPALE (DA CHIAMARE NEL LOOP) ==========
MotivoSpegnimento gestisciConnessione();

/** 
 * Loop centralizzato: gestisce client.loop() (WiFi) o polling ESP-NOW.
 * Gestisce anche il watchdog della resilienza.
 */
void loop();

/** Helper per impostare la callback di default (se prevista e non definita dal progetto) */
void setCallback();


// ========== HANDSHAKE / ANNOUNCE ==========
void sendAnnounce();

// Gestione diretta dell'ACK binario (da chiamare nella callback se topic ==
// espNowBridgeAck)
void handleAckPacket(const uint8_t *payload, size_t len);

// ========== DISPATCHER CENTRALIZZATO PacketProtocol ==========
typedef bool (*PacketHandler)(const ParsedPacket &pkt);
void registerPacketHandler(PacketHandler handler);
bool pp_dispatchPacket(const uint8_t *payload, unsigned int length);

// ========== COMANDI BINARI (PacketProtocol v2) ==========
//void sendBinaryCommand(uint8_t deviceID, bool on);// Deprecato
void sendBinaryAck(uint8_t deviceID, uint8_t command, uint8_t value);
AckState sendBinaryCommandWithAck(uint8_t deviceID, bool on,
                                  uint8_t retries = 2,
                                  uint32_t timeoutMs = 1000);

// ========== RICEZIONE TRASPORTO (PER ACK/RISPOSTE) ==========
int receive(uint8_t *buffer, size_t buflen);

// ========== SETUP COMPLETO (DA CHIAMARE IN setup()) ==========
// topics può essere nullptr se il progetto non ha topic extra oltre ai core
MotivoSpegnimento setupCompleto(IPAddress ip, const char *mqtt_id,
                                const char *topics[] = nullptr, uint8_t deviceID = 0xFF);

} // namespace mqttWifi
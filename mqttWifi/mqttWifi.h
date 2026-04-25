#pragma once
#ifdef ESP8266_BUILD
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#define httpUpdate ESPhttpUpdate

#elif ESP32_BUILD
#include "esp_bt.h"
#include "esp_bt_main.h"
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
extern PubSubClient client;

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

// trasporto selezionabile
void setMqttTransport(MqttTransportType t);
MqttTransportType getMqttTransport();

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
bool sottoscriviTopics(const char *topics[]);

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
 * Sostituisce la chiamata diretta a client.loop() nei progetti.
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
/**
 * Hook applicativo registrato dal progetto locale.
 * Viene chiamato SOLO per i pacchetti non gestiti dalla libreria
 * (comandi broadcast come SLEEP/UPDATE/RESET sono già intercettati).
 * Restituisce true se il pacchetto è stato gestito, false altrimenti.
 */
typedef bool (*PacketHandler)(const ParsedPacket &pkt);

/** Registra l'hook del progetto. Da chiamare una volta in setup(). */
void registerPacketHandler(PacketHandler handler);

/**
 * Dispatcher centralizzato — da chiamare nella callback MQTT del progetto
 * al posto di tutta la logica di validazione e switch/case.
 * Gestisce autonomamente: magic check, parse, version check,
 * CMD_SYS_SLEEP, CMD_SYS_UPDATE, CMD_SYS_RESET, TYPE_ACK.
 * Passa il controllo all'hook del progetto per tutto il resto.
 * Restituisce true se il pacchetto è stato gestito.
 */
bool pp_dispatchPacket(const uint8_t *payload, unsigned int length);

// ========== COMANDI BINARI (PacketProtocol v2) ==========
/**
 * Invia un comando binario TYPE_COMMAND standard (ID dispositivo + Stato
 * ON/OFF). Utilizzato sia per inviare comandi (dal Chrono) che come ACK di
 * conferma (dalla Caldaia).
 */
void sendBinaryCommand(uint8_t deviceID, bool on);// Deprecato

void sendBinaryAck(uint8_t deviceID, uint8_t command, bool on);
AckState sendBinaryCommandWithAck(uint8_t deviceID, bool on,
                                  uint8_t retries,
                                  uint32_t timeoutMs);
// ========== RICEZIONE TRASPORTO (PER ACK/RISPOSTE) ==========
int receive(uint8_t *buffer, size_t buflen);

// ========== SETUP COMPLETO (DA CHIAMARE IN setup()) ==========
MotivoSpegnimento setupCompleto(IPAddress ip, const char *mqtt_id,
                                const char *topics[], uint8_t deviceID = 0xFF);

} // namespace mqttWifi
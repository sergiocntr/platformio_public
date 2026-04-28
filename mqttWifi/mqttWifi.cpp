#include "mqttWifi.h"
#include "IPAddress.h"
#include "PacketProtocol.h"
#include "PubSubClient.h"
#include "log_lib.h"
#include "mqttWifi_transport.h"
#include "password.h"
#include "topic.h"

WiFiClient c_mqtt;

namespace mqttWifi {

// Variabili globali per il trasporto (accessibili da protocol.cpp tramite
// extern)
IMqttTransport *mqttTransport = nullptr;
MqttTransportType currentTransport = DEFAULT_MQTT_TRANSPORT;

PubSubClient client(c_mqtt);
volatile AckState ackStatus = NO_ACK;
volatile uint8_t expectedAckDeviceID = 0x00;

void setAckStatus(AckState s) { ackStatus = s; }

void setMqttTransport(MqttTransportType t) {
  if (mqttTransport) {
    mqttTransport->disconnect();
    delete mqttTransport;
  }
  currentTransport = t;
  mqttTransport = createMqttTransport(t);
  if (mqttTransport)
    mqttTransport->init();
}

MqttTransportType getMqttTransport() { return currentTransport; }

// ========== CONFIGURAZIONE ==========
const uint8_t MAX_TENTATIVI = 3;
const unsigned long TIMEOUT_WIFI = 12000;
const unsigned long TIMEOUT_MQTT = 12000;
const unsigned long SLEEP_TIME_US = 5ULL * 60ULL * 1000000ULL;

uint8_t tentativiWifi = 0;
uint8_t tentativiMqtt = 0;
IPAddress m_ip;
uint8_t m_deviceID = 0xFF;
char m_mqtt_id[20];
const char **m_topics = nullptr;

// ========== GESTIONE PUBBLICAZIONE ==========
bool publish(const char *topic, const char *message, bool retained) {
  if (getMqttTransport() == MqttTransportType::ESPNOW) {
    LOG_WARN("[PUBLISH] Stringa raw non consigliata in ESP-NOW. Usa binario.");
    return false;
  }

  if (!client.connected())
    return false;

  for (size_t tentativo = 0; tentativo < 3; tentativo++) {
    client.loop();
    if (client.publish(topic, message, retained))
      return true;
    delay(50);
  }
  return false;
}

bool publish(const char *topic, const uint8_t *payload, size_t length,
             bool retained) {
  if (getMqttTransport() == MqttTransportType::ESPNOW) {
    // --- LOGICA DI ROUTING (Resilient Star v4) ---
    // 1. I comandi e il tempo sono preferibilmente UNICAST (più affidabili)
    uint8_t type = (length >= 3) ? payload[2] : 0xFF;

    // 1. I comandi e il tempo (Discovery via Broadcast Pattern)
    if (type == TYPE_COMMAND || type == TYPE_TIME) {
      // Se non siamo "accoppiati" o il MAC non è noto, forziamo il BROADCAST (Handshake)
      if (!g_gateway_mac_trovato || !g_gateway_paired) {
        LOG_INFO("[PUBLISH] Handshake mode: invio BROADCAST (type: 0x%02X)",
                 type);
        return mqttTransport->sendBroadcast(payload, length);
      }

      // Se siamo accoppiati, usiamo l'Unicast efficiente
      LOG_VERBOSE(
          "[PUBLISH] Invio UNICAST a %02X:%02X:%02X:%02X:%02X:%02X\n",
          g_real_gateway_mac[0], g_real_gateway_mac[1], g_real_gateway_mac[2],
          g_real_gateway_mac[3], g_real_gateway_mac[4], g_real_gateway_mac[5]);

      if (mqttTransport->send(payload, length))
        return true;

      // Se l'Unicast fallisce a livello radio, perdiamo l'accoppiamento e proviamo broadcast
      LOG_WARN("[PUBLISH] Unicast fallito (radio), reset pairing e riprovo broadcast...");
      g_gateway_paired = false; 
      return mqttTransport->sendBroadcast(payload, length);
    }

    // 2. Gli ACK e la TELEMETRIA sono sempre BROADCAST (tutti devono sentire)
    // Questo risolve la contraddizione segnalata nel file bugs_to_solve.txt
    return mqttTransport->sendBroadcast(payload, length);
  } // <--- CHIUDE IL RAMO ESPNOW

  // RAMO MQTT/WiFi
  if (!client.connected())
    return false;

  for (size_t tentativo = 0; tentativo < 3; tentativo++) {
    client.loop();
    if (client.publish(topic, payload, length, retained))
      return true;
    delay(50);
  }
  return false;
}

// ========== LOG MOTIVO SPEGNIMENTO ==========
void logMotivoSpegnimento(MotivoSpegnimento motivo) {
  const char *msg = "";
  switch (motivo) {
  case CLEAN_SHUTDOWN:
    msg = "CLEAN SHUTDOWN";
    break;
  case PUBLISH_FALLITO:
    msg = "PUBLISH FALLITO";
    break;
  case COMANDO_SYSTEM_TOPIC:
    msg = "COMANDO SYSTEM TOPIC";
    break;
  case WIFI_TIMEOUT_CONNESSIONE:
    msg = "WiFi TIMEOUT";
    break;
  case MQTT_TIMEOUT_CONNESSIONE:
    msg = "MQTT TIMEOUT";
    break;
  case WIFI_FALLITO_SETUP:
    msg = "WiFi FALLITO SETUP";
    break;
  case SHUTDOWN_FROM_MQTT:
    msg = "SHUTDOWN FROM MQTT";
    break;
  default:
    msg = "SCONOSCIUTO";
    break;
  }
  LOG_INFO("[SLEEP] Motivo: %s", msg);
  (void)msg; // Evita warning se LOG_INFO è vuoto
}

// ========== GESTIONE RIPOSO (SOCIAL SLEEP) ==========
void adessoDormo(uint8_t mode, MotivoSpegnimento motivo) {
  logMotivoSpegnimento(motivo);

  if (getMqttTransport() != MqttTransportType::ESPNOW && client.connected()) {
    client.disconnect();
  }

  if (getMqttTransport() != MqttTransportType::ESPNOW) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }

  if (motivo == ONLY_DISCONNETS)
    return;

  LOG_INFO("[SLEEP] Entro in pausa... (Touch o Timer 5min)");
  Serial.flush();

#ifdef ESP8266_BUILD
  wifi_set_sleep_type(LIGHT_SLEEP_T);
  WiFi.forceSleepBegin();
  delay(300000);
  ESP.reset();
#elif ESP32_BUILD
  esp_sleep_enable_timer_wakeup(SLEEP_TIME_US);
  esp_deep_sleep_start();
#endif
}

// ========== SETUP WiFi ==========
void setupWifi() {
  if (WiFi.getMode() == WIFI_AP_STA || WiFi.getMode() == WIFI_STA) {
    if (WiFi.status() == WL_CONNECTED)
      return;
  }

  WiFi.persistent(false);

  if (WiFi.getMode() == WIFI_OFF) {
    WiFi.mode(WIFI_STA);
    delay(100);
  }

  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);
#ifdef ESP8266_BUILD
  WiFi.setOutputPower(17);
  WiFi.forceSleepWake();
#endif
  WiFi.config(m_ip, gateway, subnet, dns1);
}

void randomDelayAtBoot() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  delay(((mac[4] + mac[5]) % 100) * 10);
}

// ========== CONNESSIONI CON RETRY ==========
bool connectWifi() {
  if (WiFi.status() == WL_CONNECTED)
    return true;
  WiFi.begin(ssid, password);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > TIMEOUT_WIFI)
      return false;
    delay(250);
  }
  return true;
}

bool connectMqtt() {
  if (client.connected())
    return true;
  String clientId = String(m_mqtt_id) + String(random(0xffff), HEX);
  client.setServer(ipMqtt_server, mqtt_port);
  client.setBufferSize(1024);

  uint32_t start = millis();
  while (!client.connected()) {
    if (millis() - start > TIMEOUT_MQTT)
      return false;
    char statusTopic[64];
    snprintf(statusTopic, sizeof(statusTopic), "%s/status", m_mqtt_id);
    if (client.connect(clientId.c_str(), mqttUser, mqttPass, statusTopic, 0,
                       true, "offline")) {
      client.publish(statusTopic, "online", true);
      return sottoscriviTopics(m_topics);
    }
    delay(250);
  }
  return false;
}

// ========== SOTTOSCRIZIONE TOPIC (CORE + PROJECT) ==========
bool sottoscriviTopics(const char *progetto_topics[]) {
  bool success = true;

  // 1. Core topics (sempre necessari per resilienza)
  for (int i = 0; CORE_TOPICS[i] != nullptr; i++) {
    if (!client.subscribe(CORE_TOPICS[i]))
      success = false;
  }

  // 2. Project topics
  if (progetto_topics) {
    for (int i = 0; progetto_topics[i] != nullptr; i++) {
      if (!client.subscribe(progetto_topics[i]))
        success = false;
    }
  }

  sendAnnounce();
  return success;
}

MotivoSpegnimento gestisciConnessione() {
  if (getMqttTransport() == MqttTransportType::ESPNOW) {
    if (mqttTransport && mqttTransport->connect()) {
      mqttTransport->keepAlive();
      return CONN_OK;
    }
    // ESP-NOW connect() fallito: cleanup PRIMA di switchare a WiFi
    LOG_WARN("[CONN] ESP-NOW fallito, switch a WiFi con cleanup");
    if (mqttTransport) {
      mqttTransport->disconnect();
    }
    setMqttTransport(MqttTransportType::WIFI);
    setupWifi();
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (!connectWifi()) {
      return WIFI_TIMEOUT_CONNESSIONE;
    }
  }

  if (!client.connected()) {
    if (!connectMqtt()) {
      return MQTT_TIMEOUT_CONNESSIONE;
    }
  }

  loop();
  return CONN_OK;
}

void loop() {
  uint32_t now = millis();

  // Watchdog Resilienza: controlla il heartbeat TIME dal gateway
  if (lastTimeSynced != 0 && (now - lastTimeSynced > SYNC_TIMEOUT)) {
    LOG_WARN("[WATCHDOG] Heartbeat TIME perso da %lu ms!",
             (now - lastTimeSynced));

    if (getMqttTransport() == MqttTransportType::ESPNOW) {
      setMqttTransport(MqttTransportType::WIFI);
      setupWifi();
      if (connectWifi() && connectMqtt()) {
        lastTimeSynced = now;
        LOG_INFO("[WATCHDOG] Fallback a WiFi riuscito. Watchdog resettato.");
      } else {
        // Riconnessione fallita: evitiamo lo spam ma non resettiamo a 2 minuti.
        // Lo facciamo scattare di nuovo tra 30 secondi.
        lastTimeSynced = now - SYNC_TIMEOUT + 30000;
        LOG_ERROR("[WATCHDOG] Fallback fallito. Riprovo tra 30s.");
      }
    } else {
      // Già in WiFi ma niente tempo? Siamo offline.
      lastTimeSynced = now - SYNC_TIMEOUT + 30000;
      LOG_ERROR("[WATCHDOG] Persa sincronizzazione in WiFi. Riprovo tra 30s.");
    }
  }

  if (getMqttTransport() == MqttTransportType::ESPNOW) {
    uint8_t rxBuf[250];
    int rxLen;
    // Svuota TUTTA la FIFO ad ogni ciclo per evitare overrun in caso di burst
    while ((rxLen = receive(rxBuf, sizeof(rxBuf))) > 0) {
      pp_dispatchPacket(rxBuf, (unsigned int)rxLen);
    }
  } else {
    client.loop();
  }
}

__attribute__((weak)) void setCallback() {}

int receive(uint8_t *buffer, size_t buflen) {
  return mqttTransport ? mqttTransport->receive(buffer, buflen) : 0;
}

// ========== AGGIORNAMENTO FIRMWARE ==========
void checkForUpdates() {
  LOG_INFO("[UPDATE] Controllo aggiornamenti...");
  bool wasEspNow = (getMqttTransport() == MqttTransportType::ESPNOW);

  if (wasEspNow) {
    setupWifi();
    if (!connectWifi())
      return;
  }

  String fwURL = String(fwUrlBase) + m_mqtt_id;
  WiFiClient myLocalConn;
  HTTPClient httpClient;
  httpClient.begin(myLocalConn, fwURL + "/version.php");

  if (httpClient.GET() == 200) {
    if (httpClient.getString().toInt() > versione) {
      httpUpdate.update(myLocalConn, fwURL + "/firmware.bin");
    }
  }

  if (wasEspNow)
    setMqttTransport(MqttTransportType::ESPNOW);
}
MotivoSpegnimento setupCompleto(IPAddress ip, const char *mqtt_id,
                                const char *progetto_topics[],
                                uint8_t deviceID) {
  m_ip = ip;
  m_deviceID = deviceID;
  m_topics = progetto_topics;
  snprintf(m_mqtt_id, sizeof(m_mqtt_id), "%s", mqtt_id);

  // --- SCELTA TRASPORTO (Resilient Star v4) ---
  // Di default partiamo SEMPRE con ESP-NOW. È veloce, leggero e non richiede
  // connessione al router. Se il gateway non risponde, passeremo a WiFi.
#ifdef ESP32_MQTT
  // Il Gateway (Bridge) parla ovviamente MQTT/WiFi direttamente dal broker
  setMqttTransport(MqttTransportType::WIFI);
#else
  setMqttTransport(MqttTransportType::ESPNOW);
#endif

  // Inizializza il trasporto scelto
  if (getMqttTransport() == MqttTransportType::ESPNOW) {
    if (mqttTransport && !mqttTransport->connect()) {
      LOG_WARN("[SETUP] Gateway ESP-NOW non trovato. Fallback a WiFi...");
      setMqttTransport(MqttTransportType::WIFI);
      setupWifi();
    }
  } else {
    setupWifi();
  }

  return gestisciConnessione();
}
} // namespace mqttWifi
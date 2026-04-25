#include "mqttWifi.h"
#include "IPAddress.h"
#include "PacketProtocol.h"
#include "PubSubClient.h"
#include "log_lib.h"
#include "mqttWifi_transport.h"
#include "password.h"
#include "topic.h"

WiFiClient c_mqtt;
WiFiClient mywifi;

static IMqttTransport *mqttTransport = nullptr;
static MqttTransportType currentTransport = DEFAULT_MQTT_TRANSPORT;

namespace mqttWifi {
PubSubClient client(c_mqtt);
volatile AckState ackStatus = NO_ACK;
volatile uint8_t expectedAckDeviceID = 0xFF;

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
uint8_t m_deviceID = 0xFF; // ID numerico per PacketProtocol
char m_mqtt_id[20];
const char **m_topics = nullptr; // array topic fornito dal progetto

// ========== GESTIONE PUBBLICAZIONE ==========
bool publish(const char *topic, const char *message, bool retained) {
  if (getMqttTransport() == MqttTransportType::ESPNOW) {
    LOG_WARN(
        "[PUBLISH] Invio stringa raw non consigliato in ESP-NOW. Usa binario.");
    return false;
  }

  if (!client.connected()) {
    LOG_ERROR("[PUBLISH] Client non connesso");
    return false;
  }

  // Tentativi multipli con delay ridotto
  for (size_t tentativo = 0; tentativo < 3; tentativo++) {
    client.loop(); // Mantieni connessione attiva

    // publish(topic, payload, retained)
    if (client.publish(topic, message, retained)) {
      LOG_VERBOSE("[PUBLISH] OK su tentativo %d %s\n", tentativo + 1,
                  retained ? "(RETAINED)" : "");
      return true; // Successo
    }

    LOG_ERROR("[PUBLISH] Fallito tentativo %d/3\n", tentativo + 1);
    delay(50); // Breve pausa tra tentativi
  }

  // Dopo 3 tentativi falliti, entra in modalità riposo
  adessoDormo(8, PUBLISH_FALLITO);
  return false;
}

bool publish(const char *topic, const uint8_t *payload, size_t length,
             bool retained) {
  if (getMqttTransport() == MqttTransportType::ESPNOW) {
    if (mqttTransport && mqttTransport->send(payload, length)) {
      LOG_VERBOSE("[PUBLISH ESPNOW] OK\n");
      return true;
    }

    // --- CASO DISPERATO (Broadcast) ---
    // Se l'invio Unicast fallisce (Gateway non trovato o ACK mancante),
    // proviamo un broadcast come ultima spiaggia.
    LOG_WARN("[PUBLISH] Unicast fallito, provo BROADCAST disperato...");
    if (mqttTransport && mqttTransport->sendBroadcast(payload, length)) {
      LOG_INFO("[PUBLISH] Broadcast inviato con successo.");
      return true;
    }

    LOG_ERROR("[PUBLISH ESPNOW] Fallimento totale invio\n");
    return false;
  }

  if (!client.connected()) {
    LOG_ERROR("[PUBLISH BIN] Client non connesso");
    return false;
  }

  for (size_t tentativo = 0; tentativo < 3; tentativo++) {
    client.loop(); // Mantieni connessione attiva

    if (client.publish(topic, payload, length, retained)) {
      LOG_VERBOSE("[PUBLISH BIN] OK su tentativo %d %s\n", tentativo + 1,
                  retained ? "(RETAINED)" : "");
      return true; // Successo
    }

    LOG_ERROR("[PUBLISH BIN] Fallito tentativo %d/3\n", tentativo + 1);
    delay(50); // Breve pausa tra tentativi
  }

  // Dopo 3 tentativi falliti, entra in modalità riposo
  adessoDormo(8, PUBLISH_FALLITO);
  return false;
}

/**
 * @deprecated Usa sendBinaryCommandWithAck
 */
[[deprecated("Usa sendBinaryCommandWithAck")]]
void sendBinaryCommand(uint8_t deviceID, bool on) {
  cmdData d;
  d.deviceID = deviceID;
  d.command = on ? CMD_POWER_ON : CMD_POWER_OFF;
  d.value = on ? 1 : 0;

  uint8_t buffer[HEADER_SIZE + sizeof(cmdData) + 1];
  size_t packetSize =
      pp_buildPacket(TYPE_COMMAND, (uint8_t *)&d, sizeof(cmdData), buffer);

  LOG_ERROR(
      "[ERROR] sendBinaryCommand è deprecata, usare sendBinaryCommandWithAck");
  publish(espNowBridgeCmd, buffer, packetSize, false);
}

/**
 * Invia un comando binario ON/OFF con gestione ACK e retry.
 *
 * @param deviceID  ID del dispositivo target
 * @param on        true = CMD_POWER_ON, false = CMD_POWER_OFF
 * @param retries   numero di ritentativi in caso di mancato ACK (default 2)
 * @param timeoutMs timeout attesa ACK per ogni tentativo in ms (default 3000)
 * @return AckState risultato dell'ultimo tentativo
 */
AckState sendBinaryCommandWithAck(uint8_t deviceID, bool on,
                                  uint8_t retries = 2,
                                  uint32_t timeoutMs = 300) {
  expectedAckDeviceID = deviceID;
  cmdData d;
  d.deviceID = deviceID;
  d.command = on ? CMD_POWER_ON : CMD_POWER_OFF;
  d.value = on ? 1 : 0;

  uint8_t buffer[HEADER_SIZE + sizeof(cmdData) + 1];
  size_t packetSize =
      pp_buildPacket(TYPE_COMMAND, (uint8_t *)&d, sizeof(cmdData), buffer);

  for (int i = 0; i <= retries; i++) {
    ackStatus = NO_ACK;

    if (!publish(espNowBridgeCmd, buffer, packetSize, false)) {
      LOG_WARN("[CMD-ACK] Publish fallito al tentativo %d/%d", i + 1,
               retries + 1);
      delay(200);
      continue;
    }

    AckState res = waitForAck(timeoutMs);

    if (res == ACK || res == END) {
      LOG_VERBOSE("[CMD-ACK] ✓ Confermato (deviceID=0x%02X, tentativo %d)",
                  deviceID, i + 1);
      expectedAckDeviceID = 0xFF;
      return res;
    }
    if (res == ERROR) {
      LOG_ERROR("[CMD-ACK] ✗ ERROR ricevuto (deviceID=0x%02X)", deviceID);
      expectedAckDeviceID = 0xFF;
      return ERROR;
    }

    LOG_WARN("[CMD-ACK] Tentativo %d/%d senza ACK, riprovo...", i + 1,
             retries + 1);
    delay(200);
  }

  LOG_ERROR("[CMD-ACK] ✗ Nessun ACK dopo %d tentativi (deviceID=0x%02X)",
            retries + 1, deviceID);
  expectedAckDeviceID = 0xFF;
  return NO_ACK;
}

void sendBinaryAck(uint8_t deviceID, uint8_t command, bool on) {
  ackData d;
  d.deviceID = deviceID;
  d.status = ACK; // Transport ACK success
  d.cmdEcho = command;
  d.valEcho = on ? 1 : 0;

  uint8_t buffer[HEADER_SIZE + sizeof(ackData) + 1];
  size_t packetSize =
      pp_buildPacket(TYPE_ACK, (uint8_t *)&d, sizeof(ackData), buffer);

  LOG_VERBOSE("[ACK BIN] Application Confirmation for ID: 0x%02X -> %s",
              deviceID, on ? "ON" : "OFF");
  publish(espNowBridgeAck, buffer, packetSize, false);
}

AckState waitForAck(uint32_t timeoutMs) {
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    // 1. ESP-NOW Fast polling (se attivo)
    if (getMqttTransport() == MqttTransportType::ESPNOW) {
      uint8_t buf[32];
      int len = receive(buf, sizeof(buf));

      // Nuovo controllo binario v2 (TYPE_ACK = 0x00)
      if (len >= 8 && buf[0] == 0xAA && buf[2] == 0x00) {
        uint8_t rcvId = buf[5];
        if (rcvId == m_deviceID ||
            rcvId == expectedAckDeviceID) { // Verifica destinatario o atteso!
          ackStatus = (AckState)buf[6];
          LOG_VERBOSE("[ACK BIN] Ricevuto via ESP-NOW per ID 0x%02X\n", rcvId);
        }
      }
      // Retrocompatibilità stringhe legacy
      else if (len > 0) {
        if (len >= 3 && memcmp(buf, "ack", 3) == 0)
          ackStatus = ACK;
        else if (len >= 3 && memcmp(buf, "end", 3) == 0)
          ackStatus = END;
        else if (len >= 6 && memcmp(buf, "failed", 6) == 0)
          ackStatus = FAILED;
        else if (len >= 5 && memcmp(buf, "error", 5) == 0)
          ackStatus = ERROR;

        if (ackStatus != NO_ACK) {
          LOG_VERBOSE("[ACK LEGACY] Ricevuto via ESP-NOW\n");
        }
      }
    }

    // 2. MQTT and General Polling
    if (ackStatus != NO_ACK) {
      AckState result = ackStatus;
      ackStatus = NO_ACK; // Consumato
      LOG_VERBOSE("[ACK] Ricevuto via MQTT (o confermato)\n");
      return result;
    }
    client.loop(); // Gestisce la callback che potrebbe chiamare setAckStatus
    delay(10);
  }

  LOG_WARN("[ACK] Timeout attesa risposta");
  return NO_ACK;
}

AckState publishWithAck(const char *topic, const uint8_t *payload,
                        size_t length, uint8_t retries, uint32_t timeoutMs) {
  expectedAckDeviceID = m_deviceID;
  for (int i = 0; i <= retries; i++) {
    ackStatus = NO_ACK;
    if (publish(topic, payload, length)) {
      AckState res = waitForAck(timeoutMs);
      if (res == ACK || res == END) {
        expectedAckDeviceID = 0xFF;
        return res;
      }
      if (res == SWITCH_TRANSPORT) {
        setMqttTransport(MqttTransportType::ESPNOW);
        return SWITCH_TRANSPORT;
      }
      if (res == ERROR) {
        expectedAckDeviceID = 0xFF;
        return ERROR;
      }
      // Altrimenti riprova (FAILED o NO_ACK/TIMEOUT)
      LOG_WARN("[PUBLISH-ACK] Tentativo %d/%d fallito, riprovo...", i + 1,
               retries + 1);
    }
    delay(200);
  }
  expectedAckDeviceID = 0xFF;
  return NO_ACK;
}

// ========== LOG MOTIVO SPEGNIMENTO ==========
void logMotivoSpegnimento(MotivoSpegnimento motivo) {
  const char *msg = "";
  switch (motivo) {
  case CLEAN_SHUTDOWN:
    msg = "CLEAN SHUTDOWN";
    break;
  case PUBLISH_FALLITO:
    msg = "PUBLISH FALLITO dopo 3 tentativi";
    break;
  case COMANDO_SYSTEM_TOPIC:
    msg = "COMANDO via systemTopic (payload '0')";
    break;
  case WIFI_TIMEOUT_CONNESSIONE:
    msg = "WiFi TIMEOUT dopo 3 tentativi";
    break;
  case MQTT_TIMEOUT_CONNESSIONE:
    msg = "MQTT TIMEOUT dopo 3 tentativi";
    break;
  case WIFI_FALLITO_SETUP:
    msg = "WiFi FALLITO durante setup";
    break;
  case NEXTION_SETUP_FAILED:
    msg = "NEXTION INIT FAILLITO durante setup";
    break;
  case DHT_SETUP_FAILED:
    msg = "DHT INIT FAILLITO durante setup";
    break;
  case SHUTDOWN_FROM_MQTT:
    msg = "SHUTDOWN FROM MQTT";
    break;
  case ONLY_DISCONNETS:
    msg = "ONLY DISCONNECTS: non entro in deep sleep";
    break;
  default:
    msg = "SCONOSCIUTO";
    break;
  }
  LOG_INFO("[SLEEP] Motivo: %s", msg);
  (void)msg; // Suppress unused variable warning if LOG_INFO is empty
}

// ========== DEEP SLEEP ==========
void adessoDormo(uint8_t mode, MotivoSpegnimento motivo) {
  logMotivoSpegnimento(motivo);

  // Spegnimento Nextion
  if (mode > 0) {
    LOG_VERBOSE("[SLEEP] Spegnimento Nextion");
    // sendCommand("thup=1");  // gestito altrove
    // sendCommand("sleep=1");
    delay(200);
  }

  // Chiusura MQTT (solo se era connesso via WiFi)
  if (getMqttTransport() != MqttTransportType::ESPNOW && client.connected()) {
    LOG_VERBOSE("[SLEEP] Disconnessione MQTT");
    client.disconnect();
    delay(100);
  }

  // Spegnimento WiFi: in ESP-NOW non siamo mai associati a un AP,
  // saltare disconnect/off risparmia ~250ms prima del taglio ATtiny.
  if (getMqttTransport() != MqttTransportType::ESPNOW) {
    LOG_VERBOSE("[SLEEP] Spegnimento WiFi");
    delay(50);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
  } else {
    LOG_VERBOSE("[SLEEP] ESP-NOW: skip WiFi disconnect (non associato ad AP)");
  }

  if (MotivoSpegnimento::ONLY_DISCONNETS == motivo) {
    LOG_VERBOSE("[SLEEP] ONLY DISCONNECTS: non entro in deep sleep");
    return;
  }

  // Deep sleep
  LOG_VERBOSE("[SLEEP] Deep sleep per 5 minuti");
  Serial.flush();
  delay(100);
#ifdef ESP8266_BUILD
  wifi_set_sleep_type(LIGHT_SLEEP_T);
  WiFi.forceSleepBegin();
  delay(300000);
  ESP.reset();

#elif ESP32_BUILD
  esp_sleep_enable_timer_wakeup(SLEEP_TIME_US);
  esp_deep_sleep_start(); // NON ritorna mai
#endif
}

// ========== SETUP WiFi ==========
void setupWifi() {
  WiFi.persistent(false);
  WiFi.disconnect(true);

  WiFi.mode(WIFI_OFF);
  delay(200);

  WiFi.mode(WIFI_STA);
  delay(200);

  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);
#ifdef ESP8266_BUILD
  WiFi.setOutputPower(17);
  WiFi.forceSleepWake();
  LOG_INFO("[WiFi] Setup ESP8266");

#elif ESP32_BUILD
  LOG_INFO("[WiFi] Setup ESP32");
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

#endif

  WiFi.config(m_ip, gateway, subnet, dns1);
}

void randomDelayAtBoot() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  unsigned long delayMs = ((mac[4] + mac[5]) % 100) * 10;
  LOG_INFO("[SETUP] Delay casuale: %lu ms", delayMs);
  delay(delayMs);
}

// ========== CONNESSIONE WiFi CON RETRY ==========
bool connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    tentativiWifi = 0; // Reset contatore se connesso
    return true;
  }

  LOG_INFO("[WiFi] Tentativo %d/%d", tentativiWifi + 1, MAX_TENTATIVI);
  WiFi.begin(ssid, password);
  delay(200);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > TIMEOUT_WIFI) {
      LOG_WARN("[WiFi] TIMEOUT");
      tentativiWifi++;
      return false;
    }
    delay(250);
    // logSerialPrint(".");
  }

  LOG_INFO("[WiFi] ✓ Connesso a %s", WiFi.localIP().toString().c_str());

  tentativiWifi = 0; // Reset contatore
  delay(50);
  return true;
}

// ========== CONNESSIONE MQTT CON RETRY ==========
bool connectMqtt() {
  if (client.connected()) {
    tentativiMqtt = 0; // Reset contatore se connesso
    return true;
  }

  LOG_VERBOSE("[MQTT] Tentativo %d/%d\n", tentativiMqtt + 1, MAX_TENTATIVI);

  String clientId = String(m_mqtt_id) + String(random(0xffff), HEX);

  client.setServer(ipMqtt_server, mqtt_port);
  client.setBufferSize(512);

  uint32_t start = millis();
  while (!client.connected()) {
    if (millis() - start > TIMEOUT_MQTT) {
      LOG_WARN("[MQTT] TIMEOUT");
      tentativiMqtt++;
      return false;
    }

    char statusTopic[64];
    snprintf(statusTopic, sizeof(statusTopic), "%s/status", m_mqtt_id);

    // Connessione con Last Will and Testament (LWT)
    if (client.connect(clientId.c_str(), mqttUser, mqttPass, statusTopic, 0, true, "offline")) {
      LOG_VERBOSE("[MQTT] ✓ Connesso");
      client.publish(statusTopic, "online", true); // Notifica stato online
      tentativiMqtt = 0; // Reset contatore
      return sottoscriviTopics(m_topics);
    }

    delay(250);
    // logSerialPrint(".");
  }

  return false;
}

// ========== SOTTOSCRIZIONE TOPIC ==========
bool sottoscriviTopics(const char *topics[]) {
  LOG_VERBOSE("[MQTT] Sottoscrizione topic dinamici...");

  char buffer[64];
  sprintf(buffer, "%s connected | FW: v%d | PROTO: v%d", m_mqtt_id, versione,
          PACKET_VERSION);
  client.publish(logTopic, buffer);

  // Handshake binario opzionale
  sendAnnounce();
  delay(10);

  bool success = true;
  int successCount = 0;
  int totalTopics = 0;

  // Unico ciclo: sottoscrivi e conta allo stesso tempo
  for (int i = 0; topics[i] != nullptr; i++) {
    bool subscribed = client.subscribe(topics[i]);

    if (subscribed) {
      successCount++;
      LOG_VERBOSE("[MQTT] ✓ Sottoscritto: %s", topics[i]);
    } else {
      success = false;
      LOG_ERROR("[MQTT] ✗ ERRORE CRITICO: %s", topics[i]);
    }

    totalTopics++;
    client.loop();
    delay(5);
  }

  if (success) {
    LOG_VERBOSE("[MQTT] ✓ Sottoscrizioni completate: %d/%d", successCount,
                totalTopics);
  } else {
    LOG_ERROR("[MQTT] ✗ Sottoscrizioni fallite: %d/%d", successCount,
              totalTopics);
  }

  return success;
}

MotivoSpegnimento gestisciConnessione() {
  if (getMqttTransport() == MqttTransportType::ESPNOW) {
    if (mqttTransport && mqttTransport->connect()) {
      mqttTransport->keepAlive();
      return CONN_OK;
    } else {
      LOG_WARN(
          "[GESTIONE] Gateway ESP-NOW assente! Fallback a Transport WIFI...\n");
      setMqttTransport(MqttTransportType::WIFI);
      setupWifi();
#ifdef DEBUG_UDP_LOG
      udpLogBegin(); // Riparte l'infrastruttura di terra
#endif
      // Se dopo il fallback immediato vogliamo forzare un tentativo WiFi, continuiamo.
      // Altrimenti, se sappiamo che il router è off, fallirà sotto.
    }
  }

  // ── WiFi ──────────────────────────────────────────────
  while (WiFi.status() != WL_CONNECTED) // ✅ while prima dell'if
  {
    LOG_INFO("[GESTIONE] WiFi disconnesso, tentativo %d/%d", tentativiWifi + 1,
             MAX_TENTATIVI);

    if (connectWifi()) {
      LOG_INFO("[GESTIONE] WiFi connesso dopo %d tentativi", tentativiWifi + 1);
      tentativiWifi = 0;
      break; // ✅ connesso, esci dal while
    }

    tentativiWifi++;
    if (tentativiWifi >= MAX_TENTATIVI) {
      LOG_ERROR("[GESTIONE] WiFi fallito dopo %d tentativi. Strategia Notturna: dormo 5 min.", MAX_TENTATIVI);
      tentativiWifi = 0;
      adessoDormo(8, WIFI_TIMEOUT_CONNESSIONE);
      return WIFI_TIMEOUT_CONNESSIONE;
    }

    delay(500); // ✅ aspetta prima del prossimo tentativo
  }

  // ── MQTT ──────────────────────────────────────────────
  while (!client.connected()) // ✅ stesso pattern del WiFi
  {
    LOG_WARN("[GESTIONE] MQTT disconnesso, tentativo %d/%d\n",
             tentativiMqtt + 1, MAX_TENTATIVI);

    if (connectMqtt()) {
      LOG_INFO("[GESTIONE] MQTT connesso dopo %d tentativi\n",
               tentativiMqtt + 1); // ✅ fix: era tentativiWifi
      tentativiMqtt = 0;
      break;
    }

    tentativiMqtt++;
    if (tentativiMqtt >= MAX_TENTATIVI) {
      LOG_ERROR("[GESTIONE] MQTT fallito dopo %d tentativi. Strategia Notturna: dormo 5 min.\n", MAX_TENTATIVI);
      tentativiMqtt = 0;
      adessoDormo(8, MQTT_TIMEOUT_CONNESSIONE);
      return MQTT_TIMEOUT_CONNESSIONE;
    }

    delay(1000);
  }

  // ── Tutto OK ──────────────────────────────────────────
  loop();
  return CONN_OK;
}

void loop() {
  if (getMqttTransport() == MqttTransportType::ESPNOW) {
    // Polling centralizzato ESP-NOW (Case 3: consumo dati broadcast/unicast)
    uint8_t rxBuf[250];
    int rxLen = receive(rxBuf, sizeof(rxBuf));
    if (rxLen > 0) {
      pp_dispatchPacket(rxBuf, (unsigned int)rxLen);
    }
  } else {
    // WiFi / MQTT
    client.loop();
  }
}

__attribute__((weak)) void setCallback() {
  // Nota: Questa è una funzione di appoggio.
  // Se il progetto definisce la propria 'setCallback' nel namespace mqttWifi,
  // quella verrà usata grazie al linking.
}

int receive(uint8_t *buffer, size_t buflen) {
  if (mqttTransport) {
    return mqttTransport->receive(buffer, buflen);
  }
  return 0;
}

// ========== AGGIORNAMENTO FIRMWARE ==========
void checkForUpdates() {
  LOG_INFO("[UPDATE] Controllo aggiornamenti firmware...");

  // --- AUTOMAZIONE WIFI PER ESP-NOW ---
  // Se siamo in modalità ESP-NOW, dobbiamo switchare temporaneamente a WiFi
  // per poter scaricare il firmware.
  bool wasEspNow = (getMqttTransport() == MqttTransportType::ESPNOW);
  if (wasEspNow || WiFi.status() != WL_CONNECTED) {
    LOG_INFO("[UPDATE] Necessaria connessione WiFi. Attivazione...");
    setupWifi();
    if (!connectWifi()) {
      LOG_ERROR(
          "[UPDATE] ✗ Impossibile connettersi al WiFi per l'aggiornamento.");
      if (wasEspNow) {
        LOG_INFO("[UPDATE] Ripristino trasporto ESP-NOW");
        mqttTransport->init(); // Ritorna in ascolto radio
      }
      return;
    }
  }

  String fwURL = String(fwUrlBase);
  fwURL.concat(m_mqtt_id);
  String rndCache = String(random(10000)); // bypass proxy cache
  String fwVersionURL = fwURL + "/version.php?nc=" + rndCache;
  String fwImageURL = fwURL + "/firmware.bin?nc=" + rndCache;

  WiFiClient myLocalConn;
  HTTPClient httpClient;
  httpClient.begin(myLocalConn, fwVersionURL);

  int httpCode = httpClient.GET();
  if (httpCode == 200) {
    String newFWVersion = httpClient.getString();
    int newVersion = newFWVersion.toInt();

    LOG_VERBOSE("[UPDATE] Versione corrente: %d, Versione disponibile: %d\n",
                versione, newVersion);
    delay(1000);

    if (newVersion > versione) {
      LOG_INFO("[UPDATE] Nuova versione disponibile! Avvio update...");

      if (client.connected()) {
        client.disconnect();
      }
      delay(1000);

      t_httpUpdate_return ret = httpUpdate.update(myLocalConn, fwImageURL);

      switch (ret) {
      case HTTP_UPDATE_FAILED:
        LOG_ERROR("[UPDATE] ✗ Aggiornamento FALLITO: %s",
                  httpUpdate.getLastErrorString().c_str());
        break;
      case HTTP_UPDATE_NO_UPDATES:
        LOG_WARN("[UPDATE] Nessun aggiornamento disponibile");
        break;
      case HTTP_UPDATE_OK:
        LOG_INFO("[UPDATE] ✓ Aggiornamento completato - Riavvio...");
        // Il riavvio è gestito dalla libreria httpUpdate, ma per sicurezza:
        delay(500);
        ESP.restart();
        break;
      }
    } else {
      LOG_INFO("[UPDATE] Firmware già aggiornato");
    }
  } else {
    LOG_ERROR("[UPDATE] ✗ Errore HTTP: %d\n", httpCode);
  }

  httpClient.end();
  myLocalConn.stop();

  // Se siamo arrivati qui senza riavviare (update fallito o non necessario)
  // e eravamo in ESP-NOW, dobbiamo ripristinare la radio.
  if (wasEspNow) {
    LOG_INFO("[UPDATE] Ritorno in modalità ESP-NOW...");
    delay(100);
    mqttTransport->init();
  }
}

// ========== HANDSHAKE / ANNOUNCE ==========
void sendAnnounce() {
  if (m_deviceID == 0xFF)
    return;

  uint8_t buf[HEADER_SIZE + sizeof(announceData) + 1];
  announceData data;
  data.deviceID = m_deviceID;
  data.protoVersion = PACKET_VERSION;
  data.fwVersion = versione;

  size_t sz = pp_buildPacket(TYPE_ANNOUNCE, (uint8_t *)&data,
                             sizeof(announceData), buf);
  // publish(espNowBridgeBuffer, buf, sz, true); // Retained per MQTT
  publish(espNowBridgeBuffer, buf, sz,
          false); // messo a retained false per evitare doppi ack
}

void handleAckPacket(const uint8_t *payload, size_t len) {
  if (len >= 8 && payload[0] == 0xAA && payload[2] == 0x00) {
    uint8_t rcvId = payload[5];
    if (rcvId == m_deviceID || rcvId == expectedAckDeviceID) {
      uint8_t status = payload[6];
      ackStatus = (AckState)status;

      // --- AUTO-SWITCH TRANSPORT ---
      // Se il Gateway ci suggerisce di passare a ESP-NOW e siamo ancora in WiFi
      if (status == 0x05 && currentTransport != MqttTransportType::ESPNOW) {
        LOG_INFO("[ACK] Gateway richiede lo switch a ESP-NOW. Eseguo...");
        setMqttTransport(MqttTransportType::ESPNOW);
      }
      LOG_VERBOSE("[ACK BIN] Match per ID 0x%02X -> Status: %d\n", rcvId,
                  ackStatus);
    }
  }
}

// ========== DISPATCHER CENTRALIZZATO ==========
static PacketHandler s_appHandler = nullptr;

void registerPacketHandler(PacketHandler handler) { s_appHandler = handler; }

bool pp_dispatchPacket(const uint8_t *payload, unsigned int length) {

  // ── 1. Validazione frame ─────────────────────────────────────
  if (length < PACKET_MIN_SIZE || payload[0] != PACKET_MAGIC) {
    LOG_WARN("[DISPATCH] Frame errato o magic missing");
    return false;
  }
  // DEBUG: Ispezione header pacchetto arrivato
  LOG_VERBOSE("[DISPATCH] RX %u bytes | Header: %02X %02X %02X %02X %02X",
              length, payload[0], payload[1], payload[2], payload[3],
              payload[4]);

  ParsedPacket pkt;
  int rc = pp_parsePacket(payload, length, &pkt);
  if (rc != 0) {
    LOG_ERROR("[DISPATCH] Parse error %d (len=%u)", rc, length);
    return false;
  }
  if (pkt.header.version != PACKET_VERSION) {
    LOG_WARN("[DISPATCH] Versione inattesa: v%d (locale: v%d)",
             pkt.header.version, PACKET_VERSION);
  }

  // ── 2. TYPE_ACK → gestito dalla libreria + passato all'app ──────
  if (pkt.header.type == TYPE_ACK) {
    handleAckPacket(payload, length);
  }

  // ── 3. Comandi broadcast di sistema (TYPE_COMMAND) ────────────
  //    Questi vengono intercettati PRIMA che il progetto li veda,
  //    in modo da garantire uniformità su tutta la rete.
  if (pkt.header.type == TYPE_COMMAND) {
    const cmdData *d = (const cmdData *)pkt.payload;

    if (d->command == CMD_SYS_SLEEP) {
      uint8_t sec = (d->value > 0) ? d->value : 8;
      LOG_INFO("[DISPATCH] CMD_SYS_SLEEP %u s (target=0x%02X)", sec,
               d->deviceID);
      if ((d->deviceID == m_deviceID) || (d->deviceID == 0xFF)) {
        adessoDormo(sec, COMANDO_SYSTEM_TOPIC);
        return true;
      }
    }

    else if (d->command == CMD_SYS_UPDATE) {
      LOG_INFO("[DISPATCH] CMD_SYS_UPDATE (target=0x%02X)", d->deviceID);
      if ((d->deviceID == m_deviceID) || (d->deviceID == 0xFF)) {
        checkForUpdates();
        return true;
      }
    }

    else if (d->command == CMD_SYS_RESET) {
      LOG_INFO("[DISPATCH] CMD_SYS_RESET (target=0x%02X)", d->deviceID);
      if ((d->deviceID == m_deviceID) || (d->deviceID == 0xFF)) {
        delay(100);
        bool isWIFI = (getMqttTransport() == MqttTransportType::WIFI);
        if (isWIFI) {
          client.disconnect();
          client.loop();
          WiFi.disconnect();
          delay(100);
        }
        ESP.restart();
        // questo non viene eseguito perche ESP.restart() non ritorna
        return true;
      }
    }
  } // Fine blocco TYPE_COMMAND

  // ── 4. Hook progetto (tutto ciò che è project-specific) ───────
  if (s_appHandler) {
    return s_appHandler(pkt);
  }

  LOG_WARN("[DISPATCH] Pacchetto non gestito o application hook assente: type=0x%02X", pkt.header.type);
  return false;
}

// ========== SETUP COMPLETO ==========
MotivoSpegnimento setupCompleto(IPAddress ip, const char *mqtt_id,
                                const char *topics[], uint8_t deviceID) {
  m_ip = ip;
  m_topics = topics;
  m_deviceID = deviceID;
  strcpy(m_mqtt_id, mqtt_id);
#ifdef DEBUG_UDP_LOG
  logSetDeviceName(m_mqtt_id);
#endif

  if (!mqttTransport) {
    setMqttTransport(DEFAULT_MQTT_TRANSPORT);
  }

  logSerialBegin(38400); // 1️⃣ Inizializza Seriale Log (se non disabilitata)

  if (getMqttTransport() == MqttTransportType::ESPNOW) {
    LOG_INFO("[SETUP] Inizializzazione trasporto ESP-NOW");
    if (mqttTransport)
      mqttTransport->init();
  } else {
    setupWifi(); // 2️⃣ WiFi prima
#ifdef DEBUG_UDP_LOG
    udpLogBegin(); // 3️⃣ poi inizializza UDP log
#endif
  }

  LOG_INFO("========================================");
  LOG_INFO("[SETUP] Avvio mqttWifi (Transport: %d)", (int)getMqttTransport());
  LOG_INFO("========================================");

  if (getMqttTransport() == MqttTransportType::WIFI) {
    randomDelayAtBoot();
  }

  return gestisciConnessione();
}

} // namespace mqttWifi
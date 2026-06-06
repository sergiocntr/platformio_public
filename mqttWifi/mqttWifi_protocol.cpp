#include "mqttWifi.h"
#include "topic.h"

namespace mqttWifi {

const char *CORE_TOPICS[] = {
    BridgeCmd, BridgeAck,
    BridgeBuffer, // Fondamentale per i nodi in WiFi per sentire l'Ora e i
                        // Sensori MQTT
    nullptr             // Sentinella
};

uint32_t lastTimeSynced = 0;
const uint32_t SYNC_TIMEOUT = 120000; // 2 minuti

static PacketHandler s_appHandler = nullptr;
extern INetworkTransport
    *mqttTransport; // Dichiarazione esterna (dal transport o main)
extern NetworkTransportType currentTransport;
extern uint8_t expectedAckDeviceID;

void registerPacketHandler(PacketHandler handler) { s_appHandler = handler; }

} // namespace mqttWifi (temporanea uscita)

// Funzione weak per la salute dei sensori: i progetti la possono sovrascrivere.
// Viene messa nel namespace globale così i vari file main.cpp possono sovrascriverla senza problemi.
__attribute__((weak)) uint8_t getLocalHealthMask() { return 0x00; }

namespace mqttWifi {

void handleAckPacket(const uint8_t *payload, size_t len) {
  if (len >= 8 && payload[0] == 0xAA && payload[2] == 0x00) {
    uint8_t rcvId = payload[5];
    LOG_VERBOSE("[ACK] Ricevuto da ID: 0x%02X (Atteso: 0x%02X o 0x%02X)\n", rcvId, m_deviceID, expectedAckDeviceID);
    // Accetta l'ACK se è per noi, se è un Broadcast (0xFF) o se è quello che aspettiamo
    if (rcvId == m_deviceID || rcvId == 0xFF || (expectedAckDeviceID != 0x00 && rcvId == expectedAckDeviceID)) {
      uint8_t status = payload[6];
      ackStatus = (AckState)status;
      
      // Ci consideriamo "accoppiati" solo se abbiamo ricevuto un ACK per un comando che stavamo ASPETTANDO noi
      if (expectedAckDeviceID != 0x00 && rcvId == expectedAckDeviceID && rcvId != 0xFF) {
        g_gateway_paired = true;
        LOG_INFO("[PROTOCOL] Handshake riuscito con 0x%02X. Da ora solo Unicast.", rcvId);
      }

      // --- AUTO-SWITCH TRANSPORT ---
      // Se il Gateway ci suggerisce di passare a ESP-NOW e siamo ancora in WiFi
      if (status == 0x05 && getNetworkTransport() != NetworkTransportType::ESPNOW) {
        LOG_INFO("[ACK] Gateway richiede lo switch a ESP-NOW. Eseguo...");
        setNetworkTransport(NetworkTransportType::ESPNOW);
      }
      LOG_VERBOSE("[ACK BIN] Match per ID 0x%02X -> Status: %d\n", rcvId,
                  ackStatus);
    }
  }
}

bool pp_dispatchPacket(const uint8_t *payload, unsigned int length) {

  // ── 1. Validazione frame ─────────────────────────────────────
  if (length >= 3) {
    LOG_VERBOSE("[DISPATCH] Raw Header: %02X %02X %02X (Len: %u)\n",
                  payload[0], payload[1], payload[2], length);
  }

  if (length < PACKET_MIN_SIZE || payload[0] != PACKET_MAGIC) {
    return false;
  }

  ParsedPacket pkt;
  int rc = pp_parsePacket(payload, length, &pkt);
  if (rc != 0) {
    LOG_ERROR("[DISPATCH] Parsing fallito con codice: %d\n", rc);
  }
  if (rc != 0) {
    LOG_ERROR("[DISPATCH] Parse error %d (len=%u)", rc, length);
    return false;
  }

  // ── 2. Intercettazione Heartbeat (TYPE_TIME) ──────────────────
  if (pkt.header.type == 0x08) { // TYPE_TIME
    lastTimeSynced = millis();
    LOG_VERBOSE("[DISPATCH] Heartbeat TIME ricevuto. Watchdog resettato.");
    
    // Risposta automatica al Gateway per confermare la presenza e lo stato sensori
    sendBinaryAck(m_deviceID, TYPE_TIME, ::getLocalHealthMask());
  }

  // ── 3. TYPE_ACK ──────────────────────────────────────────────
  if (pkt.header.type == TYPE_ACK) {
    handleAckPacket(payload, length);
  }

  // ── 4. Comandi broadcast di sistema (TYPE_COMMAND) ────────────
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
        if (getNetworkTransport() == NetworkTransportType::WIFI_MQTT) {
          client.disconnect();
          WiFi.disconnect();
        }
        ESP.restart();
        return true;
      }
    }
  }

  // ── 3. Dispatch ad applicazione (Progetti specifici) ─────────
  if (s_appHandler) {
    return s_appHandler(pkt);
  }

  // Se è un pacchetto che non ci interessa (es. telemetria di altri) non diamo
  // errore
  return false;
}

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
  publish(BridgeBuffer, buf, sz, false);
}

void sendBinaryAck(uint8_t deviceID, uint8_t command, uint8_t value) {
  ackData d;
  d.deviceID = deviceID;
  d.status = ACK;
  d.cmdEcho = command;
  d.valEcho = value;

  uint8_t buffer[HEADER_SIZE + sizeof(ackData) + 1];
  size_t packetSize =
      pp_buildPacket(TYPE_ACK, (uint8_t *)&d, sizeof(ackData), buffer);
  publish(BridgeAck, buffer, packetSize, false);
}

AckState waitForAck(uint32_t timeoutMs) {
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    if (getNetworkTransport() == NetworkTransportType::ESPNOW) {
      // BUG FIX: svuota TUTTA la FIFO ad ogni iterazione.
      // Prima si leggeva un solo pacchetto ogni 5ms: se arrivavano in
      // sequenza Rebroadcast-Comando + Telemetria-BOILER + ACK, l'ACK
      // restava in fondo alla coda e poteva andare in overrun prima di
      // essere letto, causando timeout sistematici.
      uint8_t rxBuf[250];
      int rxLen;
      while ((rxLen = receive(rxBuf, sizeof(rxBuf))) > 0) {
        pp_dispatchPacket(rxBuf, (unsigned int)rxLen);
        if (ackStatus != NO_ACK) return ackStatus; // uscita rapida
      }
    } else {
      // Tier 2: MQTT loop standard
      client.loop();
    }

    if (ackStatus != NO_ACK) {
      return ackStatus;
    }
    delay(5);
  }

  LOG_WARN("[ACK] Timeout attesa risposta (%u ms)", timeoutMs);
  return NO_ACK;
}

AckState publishWithAck(const char *topic, const uint8_t *payload,
                        size_t length, uint8_t retries, uint32_t timeoutMs) {
  AckState lastRes = NO_ACK;
  for (int i = 0; i <= retries; i++) {
    ackStatus = NO_ACK;
    if (!publish(topic, payload, length, false)) {
      delay(100);
      continue;
    }

    lastRes = waitForAck(timeoutMs);
    if (lastRes == ACK || lastRes == END) {
      return lastRes;
    }
    if (lastRes == FAILED || lastRes == ERROR || lastRes == SWITCH_TRANSPORT) {
      return lastRes; // Esci subito in caso di errori logici (evita retry inutili)
    }
    delay(100);
  }
  return lastRes;
}

AckState sendBinaryCommandWithAck(uint8_t deviceID, bool on, uint8_t retries,
                                  uint32_t timeoutMs) {
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

    if (!publish(BridgeCmd, buffer, packetSize, false)) {
      LOG_WARN("[CMD-ACK] Publish fallito al tentativo %d/%d", i + 1,
               retries + 1);
      delay(200);
      continue;
    }

    AckState res = waitForAck(timeoutMs);

    if (res == ACK || res == END) {
      LOG_VERBOSE("[CMD-ACK] ✓ Confermato (deviceID=0x%02X, tentativo %d)",
                  deviceID, i + 1);
      expectedAckDeviceID = 0x00;
      return res;
    }
    if (res == ERROR) {
      LOG_ERROR("[CMD-ACK] ✗ ERROR ricevuto (deviceID=0x%02X)", deviceID);
      expectedAckDeviceID = 0x00;
      return ERROR;
    }

    LOG_WARN("[CMD-ACK] Tentativo %d/%d senza ACK, riprovo...", i + 1,
             retries + 1);
    g_gateway_paired = false; // Fallback al prossimo tentativo (forza Broadcast)
    delay(200);
  }

  LOG_ERROR("[CMD-ACK] ✗ Nessun ACK dopo %d tentativi (deviceID=0x%02X)",
            retries + 1, deviceID);
  g_gateway_paired = false; 
  expectedAckDeviceID = 0x00;
  return NO_ACK;
}

} // namespace mqttWifi

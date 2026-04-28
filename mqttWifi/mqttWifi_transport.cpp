#include "mqttWifi_transport.h"
#include "log_lib.h"
#include <shared_config.h>

#ifdef ESP8266_BUILD
#include <ESP8266WiFi.h>
#include <espnow.h>
extern "C" {
#include "user_interface.h"
}
#elif defined(ESP32_BUILD)
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#endif

#include <PubSubClient.h>

namespace mqttWifi {
extern PubSubClient client; // Definita in mqttWifi.cpp
extern uint8_t m_deviceID;  // Aggiunto per l'handshake

// FIFO per il rx di ESP-NOW (risolve race condition e buffer overrun)
static const size_t RX_FIFO_SLOTS = 10;
struct RxFifoSlot {
  uint8_t data[250];
  size_t len;
};
static RxFifoSlot g_rxFifo[RX_FIFO_SLOTS];
static volatile size_t g_rxFifoHead = 0;      // Scritto dalla callback
static volatile size_t g_rxFifoTail = 0;      // Letto dal loop
static volatile uint32_t g_rxFifoOverrun = 0; // Contatore overrun

bool g_gateway_mac_trovato = false;
bool g_gateway_paired = false; // Diventa true dopo il primo ACK ricevuto
uint8_t g_real_gateway_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void onInternalEspNowRx(const uint8_t *mac, const uint8_t *data, size_t len) {
  // LOG_VERBOSE("\n[ESP-NOW RX] From: %02X:%02X:%02X:%02X:%02X:%02X, Len:
  // %d\n",
  //               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (int)len);

  if (len > 0 && len <= sizeof(RxFifoSlot::data)) {
    // Calcola il prossimo slot nella FIFO
    size_t nextHead = (g_rxFifoHead + 1) % RX_FIFO_SLOTS;

    // Controlla overrun (la FIFO è piena)
    if (nextHead == g_rxFifoTail) {
      g_rxFifoOverrun++;
      // LOG_WARN("[RX-FIFO] Overrun! Pacchetto perso.");
      return;
    }

    // Scrivi nel slot corrente
    RxFifoSlot &slot = g_rxFifo[g_rxFifoHead];
    slot.len = len;
    memcpy(slot.data, data, len);

    // Avanza il head
    g_rxFifoHead = nextHead;

    if (!g_gateway_mac_trovato) {
      // Solo se il pacchetto è un TYPE_ANNOUNCE (0x01) o TYPE_TIME (0x08)
      // accettiamo il MAC come gateway. Altrimenti un broadcast a caso
      // di un altro nodo potrebbe "rubare" il ruolo di gateway.
      uint8_t type = (len >= 3) ? data[2] : 0xFF;
      if (type == 0x01 || type == 0x08 ||
          type == 0x00) { // Announce, Time o ACK
        g_gateway_mac_trovato = true;
        memcpy(g_real_gateway_mac, mac, 6);
        LOG_INFO("[TRANSPORT] Gateway trovato dal traffico: "
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

#ifdef ESP8266_BUILD
        esp_now_del_peer(g_real_gateway_mac);
        esp_now_add_peer(g_real_gateway_mac, ESP_NOW_ROLE_COMBO,
                         WIFI_CHANNEL_GATEWAY, NULL, 0);
#elif defined(ESP32_BUILD)
        esp_now_peer_info_t peerInfo = {};
        peerInfo.channel = WIFI_CHANNEL_GATEWAY;
        peerInfo.encrypt = false;
        memcpy(peerInfo.peer_addr, g_real_gateway_mac, 6);
        esp_now_add_peer(&peerInfo);
#endif
      }
    }
  }
}

// Espone contatore overrun per diagnostica
uint32_t getRxFifoOverrunCount() { return g_rxFifoOverrun; }
} // namespace mqttWifi

// -----------------------------------------------------------------------------
// WIFI TRANSPORT
// -----------------------------------------------------------------------------
class WifiTransport : public IMqttTransport {
public:
  bool init() override {
    LOG_VERBOSE("[TRANSPORT] WifiTransport init\n");
    return true;
  }

  bool connect() override {
    // La connessione WiFi effettiva è gestita dalle funzioni legacy
    // connectWifi() in mqttWifi.cpp per ora.
    return (WiFi.status() == WL_CONNECTED);
  }

  void disconnect() override { WiFi.disconnect(true); }

  bool isConnected() override { return WiFi.status() == WL_CONNECTED; }

  bool send(const uint8_t *data, size_t len) override {
    // Il trasporto WiFi delega al PubSubClient nel main code.
    return false;
  }

  bool sendBroadcast(const uint8_t *data, size_t len) override { return false; }

  int receive(uint8_t *buffer, size_t buflen) override { return 0; }

  void keepAlive() override {
    // Gestito da client.loop()
  }
};

// -----------------------------------------------------------------------------
// ESP-NOW TRANSPORT NATIVO (Basato su testNow/main.cpp)
// -----------------------------------------------------------------------------
class EspNowTransport : public IMqttTransport {
private:
  bool _initialized = false;
  bool _peerAdded = false;

public:
  bool init() override {
    if (_initialized)
      return true;

    LOG_VERBOSE("[TRANSPORT] EspNowTransport init nativo\n");

    // 1. Configurazione base WiFi: STA + Disconnect
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

#ifdef ESP8266_BUILD
    // 1. Init ESP-NOW (Regola d'Oro v4.0)
    if (esp_now_init() != 0) {
      LOG_ERROR("[TRANSPORT] ESP-NOW Init fallito\n");
      return false;
    }

    // 2. Imposta ruolo e callbacks PRIMA di cambiare canale
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_recv_cb([](uint8_t *mac, uint8_t *data, uint8_t len) {
      mqttWifi::onInternalEspNowRx(mac, data, len);
    });

    // 3. Imposta il canale DOPO l'init (Regola d'Oro v4.0)
    wifi_set_channel(WIFI_CHANNEL_GATEWAY);
    delay(100); // Permetti al radio di stabilizzarsi

    esp_now_register_send_cb([](uint8_t *mac, uint8_t status){
        // status == 0: successo
                             });

    LOG_INFO("[INIT] Canale reale (ESP8266): %d | MAC: %s", wifi_get_channel(),
             WiFi.macAddress().c_str());

#elif defined(ESP32_BUILD)
    // 2. Imposta il canale PRIMA di esp_now_init()
    esp_wifi_set_channel(WIFI_CHANNEL_GATEWAY, WIFI_SECOND_CHAN_NONE);
    delay(100); // Permetti al radio di stabilizzarsi

    // 3. Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
      LOG_ERROR("[TRANSPORT] ESP-NOW Init fallito (ESP32)\n");
      return false;
    }

    // 4. Registra callbacks
    esp_now_register_recv_cb(
        [](const uint8_t *mac, const uint8_t *data, int len) {
          mqttWifi::onInternalEspNowRx(mac, data, len);
        });

    esp_now_register_send_cb(
        [](const uint8_t *mac, esp_now_send_status_t status) {
          // status confirmation
        });

    LOG_INFO("[INIT] Canale reale (ESP32): %d | MAC: %s", WiFi.channel(),
             WiFi.macAddress().c_str());
#endif

    _initialized = true;
    return true;
  }

  bool connect() override {
    if (!_initialized)
      init();
    if (_peerAdded && mqttWifi::g_gateway_mac_trovato)
      return true;

    LOG_VERBOSE("[TRANSPORT] Aggiunta Peer Gateway e Broadcast\n");
    uint8_t bcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#ifdef ESP8266_BUILD
    esp_now_add_peer(bcastMac, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL_GATEWAY, NULL,
                     0);
    // STA MAC del gateway (noto a priori, per handshake announce)
    esp_now_add_peer(const_cast<uint8_t *>(ESPNOW_GATEWAY_MAC),
                     ESP_NOW_ROLE_COMBO, WIFI_CHANNEL_GATEWAY, NULL, 0);
    // NOTA: Il peer AP_MAC NON viene registrato a priori (è "fantasma").
    // Invece, il MAC reale del gateway viene determinato dalla callback
    // onInternalEspNowRx() durante il primo ricevimento, e aggiunto
    // dinamicamente.
#elif defined(ESP32_BUILD)
    esp_now_peer_info_t peerInfo = {};
    peerInfo.channel = WIFI_CHANNEL_GATEWAY;
    peerInfo.encrypt = false;

    memcpy(peerInfo.peer_addr, bcastMac, 6);
    esp_err_t rb = esp_now_add_peer(&peerInfo);

    memcpy(peerInfo.peer_addr, ESPNOW_GATEWAY_MAC, 6);
    esp_err_t rg = esp_now_add_peer(&peerInfo);
    if (rg != ESP_OK && rb != ESP_OK) {
      LOG_ERROR("[TRANSPORT] Errore aggiunta peer\n");
    }
#endif

    // --- MANSHAKE ATTIVO (PacketProtocol v3) ---
    // Header (5) + announceData (4) + XOR (1) = 10 bytes
    uint8_t buf[10];
    buf[0] = 0xAA; // Magic
    buf[1] = 0x03; // Protocol Version v3
    buf[2] = 0x01; // TYPE_ANNOUNCE
    buf[3] = 0x04; // Payload Length LSB (announceData is 4 bytes)
    buf[4] = 0x00; // Payload Length MSB

    // Payload (deviceID + protoVer + fwVer)
    buf[5] = mqttWifi::m_deviceID;
    buf[6] = 0x03; // protoVer
#ifdef versione
    buf[7] = (uint8_t)(versione & 0xFF);
    buf[8] = (uint8_t)(versione >> 8);
#else
    buf[7] = 0;
    buf[8] = 0;
#endif

    // XOR Checksum
    buf[9] = 0;
    for (uint8_t i = 0; i < 9; i++)
      buf[9] ^= buf[i];

    mqttWifi::g_rxFifoHead = 0;
    mqttWifi::g_rxFifoTail = 0;
    mqttWifi::g_gateway_mac_trovato = false;

    LOG_INFO("[TRANSPORT] Cerco il Gateway ESP-NOW (Announce)...\n");
    const uint32_t ANNOUNCE_INTERVAL_MS = 300;
    const uint32_t ANNOUNCE_TIMEOUT_MS =
        900; // 3 tentavi * 300ms senza bloccaggio
    uint32_t announceStart = millis();
    uint32_t lastAnnounceTime = 0;
    int announceCount = 0;

    while (!mqttWifi::g_gateway_mac_trovato &&
           (millis() - announceStart < ANNOUNCE_TIMEOUT_MS)) {
      uint32_t now = millis();

      // Invia announce ogni ANNOUNCE_INTERVAL_MS
      if ((now - lastAnnounceTime) >= ANNOUNCE_INTERVAL_MS) {
#ifdef ESP8266_BUILD
        esp_now_send(bcastMac, buf, 10);
#else
        esp_now_send(bcastMac, buf, 10);
#endif
        lastAnnounceTime = now;
        announceCount++;
        LOG_VERBOSE("[TRANSPORT] Announce %d/%d inviato", announceCount, 3);

        if (announceCount >= 3)
          break; // Massimo 3 tentativi
      }

      // Yield al sistema (cooperativo scheduling, evita watchdog timeout)
      delay(10); // Small yield per non occupare il CPU
    }

    if (mqttWifi::g_gateway_mac_trovato) {
      LOG_INFO("[TRANSPORT] Gateway Trovato e Agganciato!\n");
      _peerAdded = true;
      mqttWifi::g_rxFifoTail = mqttWifi::g_rxFifoHead;
      return true;
    } else {
      LOG_ERROR("[TRANSPORT] Nessun Gateway ESP-NOW!\n");
      _peerAdded = false;
      return false;
    }
  }

  void disconnect() override {
    if (!_initialized)
      return;

#ifdef ESP8266_BUILD
    // Rimuovi tutti i peer PRIMA di deinit (evita leak della peer list)
    uint8_t bcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_del_peer(bcastMac);
    esp_now_del_peer(const_cast<uint8_t *>(ESPNOW_GATEWAY_MAC));
    esp_now_del_peer(const_cast<uint8_t *>(ESPNOW_GATEWAY_AP_MAC));
    if (mqttWifi::g_gateway_mac_trovato) {
      esp_now_del_peer(mqttWifi::g_real_gateway_mac);
    }
    esp_now_deinit();
#elif defined(ESP32_BUILD)
    // Su ESP32 de-registra i peer prima di deinit
    esp_now_del_peer(ESPNOW_GATEWAY_MAC);
    if (mqttWifi::g_gateway_mac_trovato) {
      esp_now_del_peer(mqttWifi::g_real_gateway_mac);
    }
    esp_now_deinit();
#endif
    _initialized = false;
    _peerAdded = false;
    mqttWifi::g_gateway_mac_trovato = false;
    memset(mqttWifi::g_real_gateway_mac, 0xFF, 6);
  }

  bool isConnected() override { return _initialized && _peerAdded; }

  bool send(const uint8_t *data, size_t len) override {
    if (!connect()) {
      return false;
    }

#ifdef ESP8266_BUILD
    int res = esp_now_send(mqttWifi::g_real_gateway_mac,
                           const_cast<uint8_t *>(data), len);
    return (res == 0); // SUCCESSO
#elif defined(ESP32_BUILD)
    esp_err_t res = esp_now_send(mqttWifi::g_real_gateway_mac, data, len);
    return (res == ESP_OK);
#endif
  }

  bool sendBroadcast(const uint8_t *data, size_t len) override {
    if (!_initialized)
      init();
    uint8_t bcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#ifdef ESP8266_BUILD
    int res = esp_now_send(bcastMac, const_cast<uint8_t *>(data), len);
    return (res == 0);
#elif defined(ESP32_BUILD)
    esp_err_t res = esp_now_send(bcastMac, data, len);
    return (res == ESP_OK);
#endif
  }

  int receive(uint8_t *buffer, size_t buflen) override {
    // Leggi dalla FIFO (thread-safe per ESP con core unico)
    if (mqttWifi::g_rxFifoTail == mqttWifi::g_rxFifoHead) {
      // LOG_VERBOSE("\n[RX-FIFO] FIFO vuota (tail: %d, head: %d)\n",
      //               mqttWifi::g_rxFifoTail, mqttWifi::g_rxFifoHead);
      return 0; // FIFO vuota
    }

    // Leggi il primo elemento della FIFO
    const mqttWifi::RxFifoSlot &slot =
        mqttWifi::g_rxFifo[mqttWifi::g_rxFifoTail];
    size_t toCopy = (slot.len < buflen) ? slot.len : buflen;
    memcpy(buffer, slot.data, toCopy);

    // Avanza il tail
    mqttWifi::g_rxFifoTail =
        (mqttWifi::g_rxFifoTail + 1) % mqttWifi::RX_FIFO_SLOTS;
    LOG_VERBOSE("[RX-FIFO] Pacchetto letto dalla FIFO (tail: %d, head: %d)\n",
                  mqttWifi::g_rxFifoTail, mqttWifi::g_rxFifoHead);
    return (int)toCopy;
  }

  void keepAlive() override {
    // Nessun polling richiesto, esp_now usa le interrupt.
  }
};

// -----------------------------------------------------------------------------
// DUMMY TRANSPORT
// -----------------------------------------------------------------------------
class DummyTransport : public IMqttTransport {
public:
  bool init() override { return true; }
  bool connect() override { return true; }
  void disconnect() override {}
  bool isConnected() override { return true; }
  bool send(const uint8_t *, size_t) override { return true; }
  bool sendBroadcast(const uint8_t *, size_t) override { return true; }
  int receive(uint8_t *, size_t) override { return 0; }
  void keepAlive() override {}
};

// -----------------------------------------------------------------------------
// FACTORY
// -----------------------------------------------------------------------------
IMqttTransport *createMqttTransport(MqttTransportType type) {
  switch (type) {
  case MqttTransportType::WIFI:
    return new WifiTransport();
  case MqttTransportType::ESPNOW:
    return new EspNowTransport();
  default:
    return new DummyTransport();
  }
} // namespace mqttWifi
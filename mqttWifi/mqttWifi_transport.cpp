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
// Variabile definita in mqttWifi.cpp (stesso namespace)
extern uint8_t m_deviceID;

// Variabili e funzioni interne
static const size_t RX_FIFO_SLOTS = 10;
struct RxFifoSlot {
  uint8_t data[250];
  size_t len;
};
static RxFifoSlot g_rxFifo[RX_FIFO_SLOTS];
static volatile size_t g_rxFifoHead = 0;
static volatile size_t g_rxFifoTail = 0;
static volatile uint32_t g_rxFifoOverrun = 0;

bool g_gateway_mac_trovato = false;
bool g_gateway_paired = false;
uint8_t g_real_gateway_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void onInternalEspNowRx(const uint8_t *mac, const uint8_t *data, size_t len) {
  if (len > 0 && len <= sizeof(RxFifoSlot::data)) {
    size_t nextHead = (g_rxFifoHead + 1) % RX_FIFO_SLOTS;

    if (nextHead == g_rxFifoTail) {
      g_rxFifoOverrun = g_rxFifoOverrun + 1;
      return;
    }

    RxFifoSlot &slot = g_rxFifo[g_rxFifoHead];
    slot.len = len;
    memcpy(slot.data, data, len);
    g_rxFifoHead = nextHead;

    if (!g_gateway_mac_trovato) {
      bool isGateway = (memcmp(mac, ESPNOW_GATEWAY_MAC, 6) == 0) ||
                       (memcmp(mac, ESPNOW_GATEWAY_AP_MAC, 6) == 0);

      if (isGateway && len >= 10) { // HEADER_SIZE(5) + ackData(4) + checksum(1) = 10 bytes
        uint8_t type    = data[2]; // TYPE_ACK = 0x00
        uint8_t status  = data[6]; // ackData.status = AC_SWITCH_TO_ESPNOW (payload byte [1])
        uint8_t cmdEcho = data[7]; // ackData.cmdEcho = TYPE_ANNOUNCE   (payload byte [2])

        if (type == TYPE_ACK && status == AC_SWITCH_TO_ESPNOW &&
            cmdEcho == TYPE_ANNOUNCE) {
          g_gateway_mac_trovato = true;
          memcpy(g_real_gateway_mac, mac, 6);
          LOG_INFO("[TRANSPORT] Gateway Trovato: %02X:%02X:%02X:%02X:%02X:%02X",
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
}

uint32_t getRxFifoOverrunCount() { return g_rxFifoOverrun; }

// -----------------------------------------------------------------------------
// WIFI MQTT TRANSPORT
// -----------------------------------------------------------------------------
class WifINetworkTransport : public INetworkTransport {
public:
  bool init() override {
    LOG_VERBOSE("[TRANSPORT] WifiTransport init\n");
    delay(100);
    return true;
  }

  bool connect() override { return (WiFi.status() == WL_CONNECTED); }

  void disconnect() override { WiFi.disconnect(true); }

  bool isConnected() override { return WiFi.status() == WL_CONNECTED; }

  bool send(const uint8_t *data, size_t len) override { return false; }

  bool sendBroadcast(const uint8_t *data, size_t len) override { return false; }

  int receive(uint8_t *buffer, size_t buflen) override { return 0; }

  void keepAlive() override {}
};

// -----------------------------------------------------------------------------
// ESP-NOW TRANSPORT NATIVO
// -----------------------------------------------------------------------------
class EspNowTransport : public INetworkTransport {
private:
  bool _initialized = false;
  bool _peerAdded = false;

public:
  bool init() override {
    if (_initialized)
      return true;

    LOG_VERBOSE("[TRANSPORT] EspNowTransport init nativo\n");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

#ifdef ESP8266_BUILD
    if (esp_now_init() != 0) {
      LOG_ERROR("[TRANSPORT] ESP-NOW Init fallito\n");
      return false;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_recv_cb([](uint8_t *mac, uint8_t *data, uint8_t len) {
      mqttWifi::onInternalEspNowRx(mac, data, len);
    });

    wifi_set_channel(WIFI_CHANNEL_GATEWAY);
    delay(100);

    esp_now_register_send_cb([](uint8_t *mac, uint8_t status){});

    LOG_INFO("[INIT] Canale reale (ESP8266): %d | MAC: %s", wifi_get_channel(),
             WiFi.macAddress().c_str());

#elif defined(ESP32_BUILD)
    esp_wifi_set_channel(WIFI_CHANNEL_GATEWAY, WIFI_SECOND_CHAN_NONE);
    delay(100);

    if (esp_now_init() != ESP_OK) {
      LOG_ERROR("[TRANSPORT] ESP-NOW Init fallito (ESP32)\n");
      return false;
    }

    esp_now_register_recv_cb(
        [](const esp_now_recv_info *recv_info, const uint8_t *data, int len) {
          if (recv_info) {
            mqttWifi::onInternalEspNowRx(recv_info->src_addr, data, len);
          }
        });

    esp_now_register_send_cb(
        [](const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {});

    LOG_INFO("[INIT] Canale reale (ESP32): %d | MAC: %s", WiFi.channel(),
             WiFi.macAddress().c_str());

    // Aggiunge il peer broadcast subito in init() — necessario per sendBroadcast()
    // anche senza fare l'ANNOUNCE handshake (es. nodi one-shot)
    {
      esp_now_peer_info_t bcastPeer = {};
      bcastPeer.channel = WIFI_CHANNEL_GATEWAY;
      bcastPeer.encrypt = false;
      bcastPeer.ifidx = WIFI_IF_STA;  // FIX #12 — Interfaccia STA obbligatoria (one-shot)
      uint8_t bcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      memcpy(bcastPeer.peer_addr, bcastMac, 6);
      esp_now_add_peer(&bcastPeer);
    }
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
    esp_now_add_peer(const_cast<uint8_t *>(ESPNOW_GATEWAY_MAC),
                     ESP_NOW_ROLE_COMBO, WIFI_CHANNEL_GATEWAY, NULL, 0);
#elif defined(ESP32_BUILD)
    esp_now_peer_info_t peerInfo = {};
    peerInfo.channel = WIFI_CHANNEL_GATEWAY;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;  // FIX #12 — Interfaccia STA

    // Broadcast peer già aggiunto in init() — aggiunge solo il peer unicast del gateway
    memcpy(peerInfo.peer_addr, ESPNOW_GATEWAY_MAC, 6);
    esp_now_add_peer(&peerInfo);
#endif

    // Pacchetto ANNOUNCE
    uint8_t buf[10];
    buf[0] = 0xAA;
    buf[1] = 0x03;
    buf[2] = TYPE_ANNOUNCE;
    buf[3] = 0x04;
    buf[4] = 0x00;
    buf[5] = mqttWifi::m_deviceID;
    buf[6] = 0x03;
#ifdef versione
    buf[7] = (uint8_t)(versione & 0xFF);
    buf[8] = (uint8_t)(versione >> 8);
#else
    buf[7] = 0;
    buf[8] = 0;
#endif
    buf[9] = 0;
    for (uint8_t i = 0; i < 9; i++)
      buf[9] ^= buf[i];

    mqttWifi::g_rxFifoHead = 0;
    mqttWifi::g_rxFifoTail = 0;
    mqttWifi::g_gateway_mac_trovato = false;

    LOG_INFO("[TRANSPORT] Cerco il Gateway ESP-NOW (Announce)...\n");
    const uint32_t ANNOUNCE_INTERVAL_MS = 300;
    const uint32_t ANNOUNCE_TIMEOUT_MS = 1200;
    const uint32_t ANNOUNCE_WAIT_AFTER_LAST_MS = 300;

    uint32_t announceStart = millis();
    uint32_t lastAnnounceTime = 0;
    uint32_t lastAnnounceSentAt = 0;
    int announceCount = 0;

    while (!mqttWifi::g_gateway_mac_trovato &&
           (millis() - announceStart < ANNOUNCE_TIMEOUT_MS)) {
      uint32_t now = millis();

      if (announceCount < 3 &&
          (now - lastAnnounceTime) >= ANNOUNCE_INTERVAL_MS) {
        esp_now_send(bcastMac, buf, 10);
        lastAnnounceTime = now;
        lastAnnounceSentAt = now;
        announceCount++;
        LOG_VERBOSE("[TRANSPORT] Announce %d/3 inviato", announceCount);
      }

      if (announceCount >= 3 &&
          (millis() - lastAnnounceSentAt) >= ANNOUNCE_WAIT_AFTER_LAST_MS) {
        break;
      }

      delay(10);
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
    uint8_t bcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_del_peer(bcastMac);
    esp_now_del_peer(const_cast<uint8_t *>(ESPNOW_GATEWAY_MAC));
    esp_now_del_peer(const_cast<uint8_t *>(ESPNOW_GATEWAY_AP_MAC));
    if (mqttWifi::g_gateway_mac_trovato) {
      esp_now_del_peer(mqttWifi::g_real_gateway_mac);
    }
    esp_now_deinit();
#elif defined(ESP32_BUILD)
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
    return (res == 0);
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
    if (mqttWifi::g_rxFifoTail == mqttWifi::g_rxFifoHead) {
      return 0;
    }

    const mqttWifi::RxFifoSlot &slot =
        mqttWifi::g_rxFifo[mqttWifi::g_rxFifoTail];
    size_t toCopy = (slot.len < buflen) ? slot.len : buflen;
    memcpy(buffer, slot.data, toCopy);

    mqttWifi::g_rxFifoTail =
        (mqttWifi::g_rxFifoTail + 1) % mqttWifi::RX_FIFO_SLOTS;

    return (int)toCopy;
  }

  void keepAlive() override {}
};

// -----------------------------------------------------------------------------
// DUMMY TRANSPORT
// -----------------------------------------------------------------------------
class DummyTransport : public INetworkTransport {
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

} // namespace mqttWifi

// -----------------------------------------------------------------------------
// FACTORY
// -----------------------------------------------------------------------------
INetworkTransport *createNetworkTransport(NetworkTransportType type) {
  switch (type) {
  case NetworkTransportType::WIFI_MQTT:
    return new mqttWifi::WifINetworkTransport();
  case NetworkTransportType::ESPNOW:
    return new mqttWifi::EspNowTransport();
  default:
    return new mqttWifi::DummyTransport();
  }
}

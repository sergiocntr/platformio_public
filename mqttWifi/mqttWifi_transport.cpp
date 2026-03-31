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

// Buffer per catturare l'ultima risposta ricevuta (per gli ACK o ricezioni bidirezionali)
static uint8_t g_lastRxBuf[250];
static size_t g_lastRxLen = 0;

bool g_gateway_mac_trovato = false;
uint8_t g_real_gateway_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void onInternalEspNowRx(const uint8_t *mac, const uint8_t *data, size_t len) {
  if (len > 0) {
    g_lastRxLen = (len < sizeof(g_lastRxBuf)) ? len : sizeof(g_lastRxBuf);
    memcpy(g_lastRxBuf, data, g_lastRxLen);
    
    if (!g_gateway_mac_trovato) {
      g_gateway_mac_trovato = true;
      memcpy(g_real_gateway_mac, mac, 6);
      
#ifdef ESP8266_BUILD
      esp_now_del_peer(g_real_gateway_mac); // Per sicurezza
      esp_now_add_peer(g_real_gateway_mac, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL_GATEWAY, NULL, 0);
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

    // Configurazione base WiFi valida per entrambi fissa in STA
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

#ifdef ESP8266_BUILD
    // --- FIX CANALE ESP8266 (Metodo Promiscuous) ---
    wifi_promiscuous_enable(1);
    wifi_set_channel(WIFI_CHANNEL_GATEWAY);
    wifi_promiscuous_enable(0);

    Serial.printf("[INIT] Canale reale (ESP8266): %d | MAC: %s\n", wifi_get_channel(), WiFi.macAddress().c_str());

    if (esp_now_init() != 0) {
      LOG_ERROR("[TRANSPORT] ESP-NOW Init fallito\n");
      return false;
    }
    
    // Imposta ruolo e callbacks (devono essere impostate PRIMA di aggiungere peers)
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    
    esp_now_register_recv_cb([](uint8_t *mac, uint8_t *data, uint8_t len) {
      mqttWifi::onInternalEspNowRx(mac, data, len);
    });
    esp_now_register_send_cb([](uint8_t *mac, uint8_t status) {
      // status == 0: successo
    });

#elif defined(ESP32_BUILD)
    // --- FIX CANALE ESP32 ---
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(WIFI_CHANNEL_GATEWAY, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    
    Serial.printf("[INIT] Canale reale (ESP32): %d | MAC: %s\n", WiFi.channel(), WiFi.macAddress().c_str());

    if (esp_now_init() != ESP_OK) {
      LOG_ERROR("[TRANSPORT] ESP-NOW Init fallito (ESP32)\n");
      return false;
    }

    esp_now_register_recv_cb([](const uint8_t *mac, const uint8_t *data, int len) {
      mqttWifi::onInternalEspNowRx(mac, data, len);
    });
    esp_now_register_send_cb([](const uint8_t *mac, esp_now_send_status_t status) {
      // esp_now_send_status_t enum
    });
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
    esp_now_add_peer(bcastMac, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL_GATEWAY, NULL, 0);
    // Aggiungo anche il config di default in via conservativa
    esp_now_add_peer(const_cast<uint8_t*>(ESPNOW_GATEWAY_MAC), ESP_NOW_ROLE_COMBO, WIFI_CHANNEL_GATEWAY, NULL, 0);
#elif defined(ESP32_BUILD)
    esp_now_peer_info_t peerInfo = {};
    peerInfo.channel = WIFI_CHANNEL_GATEWAY;
    peerInfo.encrypt = false;
    
    memcpy(peerInfo.peer_addr, bcastMac, 6);
    esp_err_t rb = esp_now_add_peer(&peerInfo);
    
    memcpy(peerInfo.peer_addr, ESPNOW_GATEWAY_MAC, 6);
    esp_err_t rg = esp_now_add_peer(&peerInfo);
    if(rg != ESP_OK && rb != ESP_OK) {
       LOG_ERROR("[TRANSPORT] Errore aggiunta peer\n");
    }
#endif

    // --- MANSHAKE ATTIVO ---
    uint8_t buf[10] = {0xAA, 0x01, 0x01, 0x05, 0x00, 0xFE, 0xCA, 0xFE, 0xBA, 0xBE};
    uint8_t crc = 0;
    for (uint8_t i = 0; i < 10; i++) {
       if (i != 4) crc ^= buf[i];
    }
    buf[4] = crc;

    mqttWifi::g_lastRxLen = 0;
    mqttWifi::g_gateway_mac_trovato = false; 

    LOG_INFO("[TRANSPORT] Cerco il Gateway ESP-NOW (Announce)...\n");
    for (int i = 0; i < 3; i++) {
#ifdef ESP8266_BUILD
      esp_now_send(bcastMac, buf, 10);
#else
      esp_now_send(bcastMac, buf, 10);
#endif
      delay(300); // Attendo eventuale risposta (il callback imposterà g_gateway_mac_trovato = true)
      if (mqttWifi::g_gateway_mac_trovato) {
          break; // Ci ha risposto!
      }
    }

    if (mqttWifi::g_gateway_mac_trovato) {
       LOG_INFO("[TRANSPORT] Gateway Trovato e Agganciato!\n");
       _peerAdded = true;
       return true;
    } else {
       LOG_ERROR("[TRANSPORT] Nessun Gateway ESP-NOW!\n");
       _peerAdded = false;
       return false;
    }
  }

  void disconnect() override {
    if (!_initialized) return;
#ifdef ESP8266_BUILD
    esp_now_deinit();
#elif defined(ESP32_BUILD)
    esp_now_deinit();
#endif
    _initialized = false;
    _peerAdded = false;
  }

  bool isConnected() override { return _initialized && _peerAdded; }

  bool send(const uint8_t *data, size_t len) override {
    if (!connect()) {
      return false;
    }
    
#ifdef ESP8266_BUILD
    int res = esp_now_send(mqttWifi::g_real_gateway_mac, const_cast<uint8_t*>(data), len);
    return (res == 0); // SUCCESSO
#elif defined(ESP32_BUILD)
    esp_err_t res = esp_now_send(mqttWifi::g_real_gateway_mac, data, len);
    return (res == ESP_OK);
#endif
  }

  int receive(uint8_t *buffer, size_t buflen) override {
    if (mqttWifi::g_lastRxLen == 0)
      return 0;

    size_t toCopy =
        (mqttWifi::g_lastRxLen < buflen) ? mqttWifi::g_lastRxLen : buflen;
    memcpy(buffer, mqttWifi::g_lastRxBuf, toCopy);

    int result = (int)toCopy;
    mqttWifi::g_lastRxLen = 0; // Consumato
    return result;
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
}

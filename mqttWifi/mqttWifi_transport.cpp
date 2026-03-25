#include "mqttWifi_transport.h"
#include "log_lib.h"

#ifdef ESP8266_BUILD
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#elif defined(ESP32_BUILD)
#include <WiFi.h>
#endif

// Implementazioni minimale fornite per skeleton

class WifiTransport : public IMqttTransport {
public:
  bool init() override {
    LOG_VERBOSE("[TRANSPORT] WifiTransport init\n");
    return true;
  }

  bool connect() override {
    LOG_VERBOSE("[TRANSPORT] WifiTransport connect\n");
    // deve essere gestito esternamente via setupWifi/connectWifi in mqttWifi
    return (WiFi.status() == WL_CONNECTED);
  }

  void disconnect() override {
    LOG_VERBOSE("[TRANSPORT] WifiTransport disconnect\n");
    WiFi.disconnect(true);
  }

  bool isConnected() override {
    return WiFi.status() == WL_CONNECTED;
  }

  bool send(const uint8_t *data, size_t len) override {
    (void)data;
    (void)len;
    // Questa sezione non è usata direttamente, data la struttura PubSubClient
    return false;
  }

  int receive(uint8_t *buffer, size_t buflen) override {
    (void)buffer;
    (void)buflen;
    return 0;
  }

  void keepAlive() override {
    // nop
  }
};

class EspNowTransport : public IMqttTransport {
public:
  bool init() override {
    LOG_VERBOSE("[TRANSPORT] EspNowTransport init\n");
    return true;
  }

  bool connect() override {
    LOG_VERBOSE("[TRANSPORT] EspNowTransport connect\n");
    return true;
  }

  void disconnect() override {
    LOG_VERBOSE("[TRANSPORT] EspNowTransport disconnect\n");
  }

  bool isConnected() override {
    return true;
  }

  bool send(const uint8_t *data, size_t len) override {
    (void)data;
    (void)len;
    return false;
  }

  int receive(uint8_t *buffer, size_t buflen) override {
    (void)buffer;
    (void)buflen;
    return 0;
  }

  void keepAlive() override {
    // nop
  }
};

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

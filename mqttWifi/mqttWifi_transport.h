#pragma once

#include <stdint.h>
#include <stddef.h>

// Trasport layer per la libreria mqttWifi (astrazione over WiFi/ESPNOW).

enum class MqttTransportType {
  WIFI,
  ESPNOW,
  DUMMY
};

class IMqttTransport {
public:
  virtual ~IMqttTransport() {}

  virtual bool init() = 0;
  virtual bool connect() = 0;
  virtual void disconnect() = 0;
  virtual bool isConnected() = 0;

  virtual bool send(const uint8_t *data, size_t len) = 0;
  virtual int receive(uint8_t *buffer, size_t buflen) = 0;

  virtual void keepAlive() = 0;
};

#include <shared_config.h>

IMqttTransport *createMqttTransport(MqttTransportType type);

#if defined(USE_MQTT_ESPNOW)
static constexpr MqttTransportType DEFAULT_MQTT_TRANSPORT = MqttTransportType::ESPNOW;
#elif defined(USE_MQTT_WIFI) || defined(MQTT_TRANSPORT_WIFI)
static constexpr MqttTransportType DEFAULT_MQTT_TRANSPORT = MqttTransportType::WIFI;
#else
static constexpr MqttTransportType DEFAULT_MQTT_TRANSPORT = MqttTransportType::WIFI;
#endif


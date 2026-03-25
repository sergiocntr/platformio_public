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

IMqttTransport *createMqttTransport(MqttTransportType type);

#ifdef MQTT_TRANSPORT_TYPE
static constexpr MqttTransportType DEFAULT_MQTT_TRANSPORT = MQTT_TRANSPORT_TYPE;
#else
static constexpr MqttTransportType DEFAULT_MQTT_TRANSPORT = MqttTransportType::WIFI;
#endif


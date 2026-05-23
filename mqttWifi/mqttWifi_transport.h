#pragma once

#include <stddef.h>
#include <stdint.h>

// Trasport layer per la libreria mqttWifi (astrazione over WiFi/ESPNOW).

// Dichiara l'enum a livello globale (o dentro un namespace separato)
enum class NetworkTransportType { WIFI_MQTT, ESPNOW, DUMMY, MATTER };

class INetworkTransport {
public:
  virtual ~INetworkTransport() {}

  virtual bool init() = 0;
  virtual bool connect() = 0;
  virtual void disconnect() = 0;
  virtual bool isConnected() = 0;

  virtual bool send(const uint8_t *data, size_t len) = 0;
  virtual bool sendBroadcast(const uint8_t *data, size_t len) = 0;
  virtual int receive(uint8_t *buffer, size_t buflen) = 0;

  virtual void keepAlive() = 0;
};

#include <PacketProtocol.h>
#include <shared_config.h>

// Forward declaration (senza namespace)
INetworkTransport *createNetworkTransport(NetworkTransportType type);

#if defined(USE_MQTT_ESPNOW)
static constexpr NetworkTransportType DEFAULT_NETWORK_TRANSPORT =
    NetworkTransportType::ESPNOW;
#elif defined(USE_MQTT_WIFI) || defined(MQTT_TRANSPORT_WIFI)
static constexpr NetworkTransportType DEFAULT_NETWORK_TRANSPORT =
    NetworkTransportType::WIFI_MQTT;
#else
static constexpr NetworkTransportType DEFAULT_NETWORK_TRANSPORT =
    NetworkTransportType::WIFI_MQTT;
#endif
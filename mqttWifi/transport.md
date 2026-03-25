# MQTT/WiFi/ESPNOW transport abstraction

Questa documentazione descrive lo scheletro architetturale per rendere `mqttWifi` una libreria 
agnostica rispetto al trasporto (WiFi+MQTT, ESP-NOW, futuro CAN/LTE).

## Concetto

1. `mqttWifi` fornisce l'API di alto livello per l'applicazione:
   - `setupCompleto(...)`
   - `connectWifi()` / `connectTransport()`
   - `connectMqtt()` / `connectBroker()`
   - `publish(topic, msg, retained)`
   - `sottoscriviTopics(...)`
   - `gestisciConnessione()`

2. Trasporto è un plugin che implementa contratti:
   - `init()`
   - `connect()`
   - `isConnected()`
   - `send(data)`
   - `receive(...)`
   - `sleep()` / `wake()` (opzionale)

3. In fase di build si seleziona un trasporto con `build_flags`:
   - `-DMQTT_TRANSPORT_WIFI`
   - `-DMQTT_TRANSPORT_ESPNOW`

4. `mqttWifi` mantiene il state-machine e la logica MQTT (retry, keepalive, backoff) senza sapere il dettaglio del link.

---

## Struttura file consigliata (Cartella `mqttWifi`)

- `mqttWifi.h`
- `mqttWifi.cpp`
- `mqttWifi_transport.h` (API astratta)
- `mqttWifi_transport_wifi.cpp`
- `mqttWifi_transport_esnow.cpp`
- `mqttWifi_transport_dummy.cpp` (test)
- `transport.md` (questo documento)

---

## `mqttWifi_transport.h`

Definizioni minime:

```cpp
#pragma once

enum class MqttTransportType {
    WIFI,
    ESPNOW,
    DUMMY,
};

class IMqttTransport {
public:
    virtual ~IMqttTransport() {};
    virtual bool init() = 0;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;

    // binario sincrono
    virtual bool send(const uint8_t *data, size_t len) = 0;
    virtual int receive(uint8_t *buffer, size_t buflen) = 0;

    virtual void keepAlive() = 0; // opzionale
};

IMqttTransport* createTransport(MqttTransportType type);
```

---

## In `mqttWifi.cpp` (schizzo logica)

- `mqttWifi::setupCompleto` sceglie il trasporto:
  - `mqttTransport = createTransport(MqttTransportType::WIFI)` (o ESPNOW)
- `connectWifi` -> `mqttTransport->connect()`
- `publish()` -> `mqttTransport->send(...)` quindi esegue Publish MQTT tramite `PubSubClient`/wrapper
- `gestisciConnessione` chiama `mqttTransport->receive(...)` se attivo

Il codice MQTT rimane lo stesso, il trasporto cambia.

---

## Build flags PIO

In `platformio.ini`:

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino

build_flags =
    -DMQTT_TRANSPORT_WIFI
    -DARDUINO=10805

; per ESPNOW
; build_flags =
;    -DMQTT_TRANSPORT_ESPNOW
```

---

## Regole di ingaggio per l'applicazione (consumer)

1. Scrivere in funzione unificata
   ```cpp
   mqttWifi::setupCompleto(ip, id, topics);
   mqttWifi::gestisciConnessione();
   mqttWifi::publish("/stat", "OK");
   ```
2. Non usare `WiFi` o `esp_now` direttamente se possibile, ma passare tramite `mqttWifi`.
3. Se necessario, esporre `mqttWifi::setTransport(MqttTransportType t)` prima di `setupCompleto`.
4. Lo stato e error codes sono definiti in `mqttWifi.h` (e.g. `MotivoSpegnimento`, `CONN_OK`).

---

## Compatibility con la libreria esistente

- `log_lib` può continuare a includere `ESP8266WiFi.h` (stato debug UDP) purché ci siano `#ifdef ESP8266_BUILD` e `#elif ESP32_BUILD`.
- In un futuro `log_lib` potrebbe usare un modulo `network_status` che non dipende da WiFi, ma riceve solo `bool connected` e `IPAddress`.

---

## Note finali

- Nel refactor è consigliato tenere basso il coupling: l'unico modulo che sa di MQTT è `mqttWifi`, e l'unico che sa di Link-layer è `transport`.
- `PacketProtocol` resta indipendente e ideale come carrier comune (anche per ESP-NOW e TCP). 

---

## ToDo

- [ ] implementare `IMqttTransport`
- [ ] adattare `mqttWifi` a `createTransport()` e `transport->send/receive`
- [ ] aggiungere `-DMQTT_TRANSPORT_ESPNOW` al PIO test env
- [ ] test end-to-end WiFi + ESPNOW

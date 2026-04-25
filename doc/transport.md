# MQTT/WiFi/ESPNOW transport abstraction

Questa documentazione descrive l'architettura della libreria `mqttWifi`, resa 
agnostica rispetto al trasporto fisico (WiFi+MQTT o ESP-NOW) tramite un layer di astrazione.

## Concetto

1. `mqttWifi` fornisce l'API di alto livello per l'applicazione:
   - `setupCompleto(...)`: Inizializza il trasporto e la connessione.
   - `publish(topic, msg, retained)`: Invia dati (stringa o binario).
   - `gestisciConnessione()`: Gestisce la state-machine della connessione nel loop.
   - `registerPacketHandler()`: Permette al progetto di registrare la propria logica di gestione pacchetti.

2. Il Trasporto è un'interfaccia (`IMqttTransport`) che implementa i contratti:
   - `init()`: Setup hardware (WiFi mode, ESP-NOW init).
   - `connect()`: Handshake o associazione AP.
   - `send(data)`: Invio fisico del pacchetto.
   - `receive(...)`: Recupero dati dal buffer hardware.

3. Selezione del Trasporto:
   Viene effettuata tramite `setMqttTransport(MqttTransportType)`. Il default è solitamente impostato via build flags o `shared_config.h`.

---

## Logica Interna (Namespace `mqttWifi`)

- `mqttWifi::setupCompleto` istanzia il trasporto corretto:
  - `mqttTransport = createMqttTransport(type)`.
- `publish()` delega a `mqttTransport->send(...)`.
- `pp_dispatchPacket()` viene alimentato dai dati ricevuti da `mqttTransport->receive(...)`.

Il codice MQTT di alto livello rimane lo stesso, indipendentemente dal fatto che il bit viaggi via TCP/IP o via frame ESP-NOW.

---

## Esempio di Utilizzo nei Progetti

```cpp
// In setup()
mqttWifi::setCallback(); // Hook per la gestione pacchetti !PRIMA della connessione
mqttWifi::setupCompleto(ip, id, topics, deviceID);


// In loop()
mqttWifi::loop(); // 👈 Centralizzato: gestisce MQTT loop o polling ESP-NOW
mqttWifi::gestisciConnessione(); // Opzionale: per supervisione watchdog
```

---

## Compatibilità e Dipendenze

- `PacketProtocol` è il carrier universale usato da tutti i trasporti.
- `log_lib` utilizza il trasporto attivo per inviare log (UDP se WiFi, bufferizzato se ESP-NOW).
- `shared_config.h` definisce il trasporto di default per l'intera rete per garantire omogeneità.

---

## Note Architetturali

- **Basso accoppiamento**: L'applicazione non deve sapere se sta usando WiFi o ESP-NOW.
- **Resilienza (v3.0+)**:
    - **Fallback ESP-NOW → WiFi**: La libreria passa a WiFi se il gateway non risponde.
    - **Broadcast Disperato**: Se l'Unicast fallisce, il nodo ritrasmette in Broadcast come ultima spiaggia.
    - **Rebroadcast Gateway (Caso 3)**: Il Gateway rilancia i dati dei sensori per i nodi offline.

---

## Strategia Notturna e Resilienza (v3.2+)

### 1. MQTT Last Will and Testament (LWT)
Ogni nodo (via WiFi) o il Gateway registra un testamento sul topic `[mqtt_id]/status`:
- `online` (retained): Dispositivo connesso e operativo.
- `offline`: Pubblicato dal broker se il dispositivo perde la connessione (es. blackout o shutdown).

### 2. Auto-Sleep Ciclico
Per gestire lo shutdown notturno di router e broker (RPi):
- Se `gestisciConnessione()` fallisce tutti i tentativi (ESP-NOW e WiFi), il nodo entra in **Deep Sleep per 5 minuti**.
- Al risveglio, il nodo riprova il ciclo di connessione. Questo minimizza il consumo energetico e lo stress della radio durante le ore di inattività del backend.

---

## Build Flags consigliate per nodi ESP-NOW in produzione

I nodi con `MqttTransportType::ESPNOW` non hanno connessione WiFi ad un AP: il log UDP è **strutturalmente impossibile** ma senza flag esplicite il relativo codice viene compilato e linkato inutilmente.

### `platformio.ini` — sezione `[common]`

```ini
build_flags =
    -DDISABLE_UDP_LOG   ; esclude WiFiUDP, UdpLogger e char buf[256] a compile-time
    -DDEBUG_LEVEL=1     ; produzione: solo WARN + ERROR (elimina format string INFO/VERBOSE da Flash)
```

| Simbolo eliminato | RAM/Flash risparmiata |
|---|---|
| `WiFiUDP udpLog` (globale) | ~340B RAM |
| `char buf[256]` in `udpLogSend_f` | 256B stack per ogni LOG call |
| Format string `LOG_INFO/VERBOSE` | ~9KB Flash (su ESP8266) |

> **Affidabilità**: su ESP8266 (80KB RAM) ridurre stack pressure e heap fragmentation ha impatto diretto sulla stabilità a lungo termine, specialmente con uptime di giorni/settimane.

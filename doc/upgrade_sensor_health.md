# Piano di Upgrade: Sensor Health & Centralized Sensor Management

Questo documento descrive la strategia per centralizzare la gestione dei sensori (DHT22, DS18B20, PZEM, MAX31855) e implementare il monitoraggio dello stato di "salute" dei sensori in tutta la rete Resilient Star.

## Obiettivi
1.  **Centralizzazione**: Spostare la logica di inizializzazione e lettura dei sensori in una libreria pubblica (`SensorManager`).
2.  **Monitoraggio**: Trasmettere lo stato `sensor_alive` dai nodi al gateway tramite il heartbeat (ACK).
3.  **Visibilità**: Visualizzare la salute dei sensori nel report dei nodi del gateway.

---

## Stato Avanzamento

- [x] **Fase 1: Libreria SensorManager**
    - [x] Creazione struttura `platformio_public/SensorManager`.
    - [x] Implementazione modulo `DhtManager`.
    - [x] Implementazione modulo `Ds18Manager`.
    - [x] Implementazione modulo `PzemManager`.
    - [x] Implementazione modulo `Max31855Manager`.
- [x] **Fase 2: Protocollo & Gateway**
    - [x] Aggiornamento `PacketProtocol.h`: bitmask salute in `ackData` (valEcho).
    - [x] Aggiornamento `PacketProtocol.h`: bitmask salute in `nodeEntryCompact` (flags).
    - [x] Gateway: Aggiornamento `NodeRecord` per memorizzare la salute.
    - [x] Gateway: Estrazione salute dagli ACK del heartbeat.
- [x] **Fase 3: Migrazione Nodi**
    - [x] Migrazione `NodeCaldaia`.
    - [x] Migrazione `Chrono2` (Salotto/Camere).
    - [x] Migrazione `EnergyMain`.
    - [x] Migrazione `ESP_Caminetto`.
- [x] **Fase 4: Testing & Dashboard**
    - [x] Verifica ricezione bitmask su MQTT.
    - [x] Test di simulazione guasto (scollegamento sensore).

---

## Dettagli Tecnici

### Trasporto Health Bitmask
Verrà utilizzata una bitmask di 4 bit mappata sul campo `valEcho` della struttura `TYPE_ACK` quando inviata in risposta a un `TYPE_TIME` (Heartbeat).

L'invio è automatizzato in `mqttWifi_protocol.cpp`. Ogni progetto può definire la maschera implementando la funzione:
```cpp
uint8_t getLocalHealthMask() {
    return SensorManager::getHealthMask();
}
```

### Nodi One-Shot (es. Raspberry Pi)
Per i nodi che non rimangono in ascolto (esecuzioni periodiche via script), la salute viene riportata inviando un pacchetto `TYPE_ACK` subito dopo i dati:
- `cmdEcho`: impostato a `0x08` (TYPE_TIME) per simulare la risposta a un heartbeat.
- `valEcho`: maschera di salute del sensore.

| Bit | Sensore (Standard) | Descrizione |
| :--- | :--- | :--- |
| 0 | Sensore 0 | Tipicamente DHT (Temp/Hum) o DS18B20 (Water) |
| 1 | Sensore 1 | Secondo sensore (es. Sonda K nel caminetto) |
| 2 | Sensore 2 | Terzo sensore |
| 3 | Sensore 3 | Quarto sensore |

### Libreria SensorManager
La libreria espone classi manager (`DhtManager`, `Ds18Manager`, ecc.) che aggiornano automaticamente `stato.sensor_alive[i]` in `shared_config.h`.

---
*Ultimo aggiornamento: 2026-04-30*

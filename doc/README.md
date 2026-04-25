# Mie Librerie Condivise
- **Current Version:** 3.3.0 (2026-04-24)

**Libreria unificata per ecosistema IoT (ESP8266/ESP32) con gestione trasporti agnostica (MQTT/WiFi/ESP-NOW).**

## 📖 Documentazione Tecnica
- **[Changelog](./changelog.md)**: Storico delle versioni e modifiche.
- **[Packet Protocol Reference](./packet_protocol_reference.md)**: Dettaglio del frame binario e dei tipi di pacchetto.
- **[Protocollo v3 ESP-NOW](./protocollo_v3_espnow.md)**: Dettaglio su **Smart Radio Handoff**, AC_SWITCH_TO_ESPNOW e CMD_GET_NODES.
- **[Transport Abstraction](./transport.md)**: Come funziona il layer di trasporto (`mqttWifi`).
- **[Roadmap](./roadmap.md)**: Stato di avanzamento della migrazione e prossimi passi.

## 📦 Contenuto
- `PacketProtocol`: Protocollo binario leggero e deterministico.
- `mqttWifi`: Gestore connessione con supporto automatico ESP-NOW / WiFi MQTT.
- `log_lib`: Manager di log remoto (UDP/MQTT).
- `NexManager`: Driver semplificato per display Nextion.
- `shared_config`: Definizioni globali condivise tra nodi.

## 🚀 Utilizzo

Aggiungi il percorso nel `platformio.ini`
```ini
# platformio.ini
lib_deps = 
    https://github.com/sergiocntr/platformio_libs/raw/refs/heads/master/packages/NexManager-1.0.2.tar.gz
```

**Alla compilazione riceverai un warning se la versione non corrisponde.**

***“Un programmatore testardo è qualcuno che quando arriva in un vicolo cieco, invece di tornare indietro, si costruisce una porta...*** 

***e poi magari ci mette anche una maniglia e un campanello.”***
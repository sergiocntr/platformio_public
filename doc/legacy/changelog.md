## VERSIONE 3.3.1 (2026-04-25)
**Refactoring Node-RED: Unified Meteo Parser**
- **Architettura Single Source of Truth**: Sostituiti in via definitiva i vecchi script e nodi separati (`single_meteo_parser.json`, `multi_meteo_parser.json` e il vecchio `mariner.js`) a favore del nuovo `unified_meteo_parser.json`.
- **Delegazione dell'ACK_END**: Il `buffer_parser` nativo Node-RED non emette più un prematuro `ACK_END` all'arrivo dei pacchetti meteo. Li inoltra 1:1, delegando la chiusura del ciclo in modo strettamente reattivo (ACK generato solo al rilascio del codice HTTP 200/201 dal Cloud).
- **Gestione Unificata Array Meteo**: La chiamata HTTP verso Altervista e la persistenza JSON automatica su fallback avvengono con la medesima struttura, sia per singoli record (16 byte) che frame in multi-chunk (sessione da 66 byte).
- **Protezione Strict Globals**: Implementato `requireGlobal` per impedire errori furtivi derivati da costanti non configurate.

## VERSIONE 3.3.0 (2026-04-24)
**Smart Radio Handoff & Robust Tracking**
- **Smart Radio Handoff**: Implementato meccanismo di switch automatico. Il Gateway risponde ai pacchetti `ANNOUNCE` via radio con `status = AC_SWITCH_TO_ESPNOW` (0x05) per forzare i nodi WiFi a passare alla modalità radio e disconnettersi dall'AP.
- **Robust Node Tracking**: Risolto bug di identificazione nodi (ghost 0xFE). Ora il Gateway distingue correttamente tra `deviceID` come sorgente (dati) e come target (comandi/ack) durante il tracking dei MAC address.
- **MQTT Buffer**: Aumentata la dimensione del buffer `PubSubClient` a **2048 byte** per supportare report JSON completi della tabella nodi (fino a 20+ nodi).
- **Stabilità Bridge**: introdotta protezione contro il loopback MQTT: il Gateway ignora aggiornamenti via buffer per device già marcati come `isEspNow` nell'ultimo intervallo di tempo.

## VERSIONE 3.2.0 (2026-04-24)
## VERSIONE 3.1.2 (2026-04-23)
**Housekeeping & Documentazione**
- **Topic Cleanup**: Rimossa dichiarazione duplicata di `espNowBridgeBuffer` in `topic.h`.
- **Protocol Documentation**: Aggiornato `protocollo_v3_espnow.md` con dettaglio sul meccanismo di fallback automatico (Manshake -> WiFi fallback).
- **Header Alignment**: Allineati commenti e versioni in `PacketProtocol.h` (Version 0x03).
- **Fix Ordine Setup**: Corretto ordine di inizializzazione `setCallback` vs `setupCompleto` nel firmware `chrono2/src_crono_bagno`.

## VERSIONE 3.0.0 (2026-04-19)
**Architettura Resilient Star & Fallback Broadcast**
- **PacketProtocol v3**: Protocollo ufficialmente portato alla versione 3 per supportare la logica di resilienza estesa.
- **Gateway Rebroadcast (Caso 3)**: Implementata funzione di ripetitore nel Gateway: i pacchetti Unicast dai sensori vengono rilanciati in Broadcast sulla rete ESP-NOW, garantendo visibilità ai nodi offline.
- **Broadcast Disperato**: `mqttWifi` ora esegue un fallback automatico in broadcast se l'invio unicast al gateway fallisce (ultima spiaggia).
- **Loop Centralizzato**: Introduzione di `mqttWifi::loop()` per gestire internamente il polling ESP-NOW e il client MQTT.
- **Master Ingress (Caso 4)**: Ufficializzata la gestione dei flussi MQTT -> ESP-NOW con l'introduzione di **`DEV_MASTER` (0xFF)** per la sincronizzazione temporale e i comandi di sistema broadcast.
- **UDP Logging**: Gateway migrato a `log_lib` per logging remoto centralizzato.

## SESSIONE 2026-04-19 — energyMain: Switch ESP-NOW + ottimizzazioni produzione

### EspNowGateway
- **Verifica v2.1**: Confermato allineato al protocollo. Nessuna modifica richiesta.
  - Forward binario raw su `espNowBridgeBuffer` (retain=false)
  - ANNOUNCE: solo learning peer, non pubblicato su MQTT (by design)
  - ACK bidirezionale: `espNowBridgeAck` → ESP-NOW unicast al mittente originale

### energyMain (`0x40`) — Migrazione ESP-NOW
- **Trasporto**: switch da WiFi/MQTT a **ESP-NOW nativo** (rimossa flag `-DESP32_MQTT`).
- **deviceID**: passato `DEV_ENERGYMAIN_PZEM` (`0x40`) a `setupCompleto()` per ANNOUNCE e ACK matching.
- **Heartbeat**: aggiunto timer 5 minuti in `sensors.cpp` che forza l'invio del pacchetto PZEM anche senza variazione ≥10%, necessario per il "last seen" watchdog su Node-RED.
- **Strategia monitoring**: concordata architettura senza overhead aggiuntivo — Node-RED filtra `deviceID=0x40` su `espNowBridgeBuffer` come heartbeat implicito. Nessun nuovo tipo pacchetto necessario.

### nodecaldaia (`0xE0`) — Migrazione Resilient Star
- **Trasporto**: switch a **ESP-NOW full** (rimossa flag `-DESP32_MQTT`).
- **Log**: disabilitato UDP logging locale; tracciamento delegato al Gateway (Case 3: Rebroadcast).
- **Loop**: migrazione a `mqttWifi::loop()` per gestione centralizzata del polling radio.
- **Protocollo**: allineamento ufficiale a **v3.0** (Header 0x03).

### Chrono2 (`0x10`, `0x11`) — Migrazione Resilient Star
- **Trasporto**: switch a **ESP-NOW nativo** (rimossa flag `-DESP32_MQTT`).
- **Standardizzazione**: aggiornati topic di sottoscrizione (`Cmd`, `Ack`, `Buffer`) e callback per piena sincronia con Gateway v3.
- **Protocollo**: allineamento ufficiale a **v3.0** (Header 0x03).
- **Cleanup**: risolta redefinizione `DEBUG_LEVEL` e rimossi residui log UDP non necessari.

### mqttWifi (Libreria v3.1.0)
- **Smart Radio Update**: introdotta logica di "self-healing" OTA. Se un nodo riceve `CMD_SYS_UPDATE` mentre è in ESP-NOW, attiva automaticamente il WiFi, esegue l'update e ripristina lo stato precedente (o riavvia).
- **Topic Standard**: separazione flussi MQTT per bridge Gateway:
  - `espNowBridgeCmd`: comandi in entrata (MQTT -> Radio).
  - `espNowBridgeAck`: conferme in entrata (MQTT -> Radio).
  - `espNowBridgeBuffer`: telemetria/status.
- **Linker Fix**: marcata `mqttWifi::setCallback()` come **weak** per permettere l'override da parte dei progetti senza conflitti di linking.

### Ottimizzazioni compile-time (ESP-NOW production)
- Flag `-DDISABLE_UDP_LOG`: esclude a compile-time `WiFiUDP`, `UdpLogger`, `char buf[256]` (impossibile in ESP-NOW, altrimenti compilato ma inutilizzato).
- Flag `-DDEBUG_LEVEL=1`: mantiene solo `LOG_WARN` e `LOG_ERROR`, elimina le format string di INFO/VERBOSE dal Flash.
- **Risparmio**: −3.6KB RAM, −9KB Flash vs configurazione originale.
- **Affidabilità**: riduzione stack pressure e heap fragmentation a lungo termine.

### mqttWifi (libreria)
- **Fix**: aggiunte guardie `#ifdef DEBUG_UDP_LOG` attorno a `udpLogBegin()` e `logSetDeviceName()` in `mqttWifi.cpp`. Nessun impatto sui progetti con WiFi standard.

---

## VERSIONE 2.1.0 (2026-04-12)
**Sincronizzazione Applicativa & Cleanup**
- **Application ACK v2.1**: Introdotto `cmdEcho` e `valEcho` in `TYPE_ACK` per sincronizzazione perfetta tra attuatori e display.
- **Migrazione Chrono**: Completata la transizione a PacketProtocol per i nodi `src_crono` e `src_crono_bagno`.
- **Single Source of Truth**: Centralizzata tutta la documentazione tecnica nella cartella `platformio_public/doc`.
- **Topologia di Sistema**: Creato il documento `system_topology.md` per mappare l'architettura della rete.

## VERSIONE 1.5.0
**Distibuzione librerie come packages**
- piu comodo da gestire in PlatformIO
- ogni progetto scarica solo la libreria che serve ,non tutto il repo
- ogni progetto puo usare una specifica verione della libreria
## VERSIONE 1.4.0
- Aggiunto library.json per ogni libreria
- Aggiunta cartella `shared_config` per dichiarazioni condivise
- altri piccoli fix
## VERSIONE 1.2.0
### Modifica nella libreria ESPManager
- Rimosse tutte le dipendenze esterne — niente impostazioni_chrono.h, NexManager.h, stato, EspPacket, broadcastAddress, gatewayAddress. 
 - La libreria non sa nulla dell'applicazione (agnostica).
**FIFO circolare (ring buffer)**
- La callback ISR-like di ESP-NOW scrive nello slot tail e incrementa il puntatore — nient'altro. poll() legge da head, copia i dati su stack locale, avanza head prima di chiamare la tua callback, così la callback stessa può chiamare send() senza problemi.
**Gestione peer multipli**
- Array statico di PeerInfo[20] (limite hardware ESP-NOW). Le funzioni addPeer / removePeer / hasPeer / peerCount / getPeers mantengono sincronizzati la lista interna e lo stack ESP-NOW.
**Statistiche**
- La struct Stats traccia rxTotal, rxDropped, txTotal, txFailed, txDelivered — utile per diagnosticare perdite di pacchetti o problemi di consegna senza Serial.print sparsi.

**Nota**

La libreria è pulita e dovrebbe integrarsi senza problemi. 
Quando la provi, tieni d'occhio i rxDropped nelle statistiche — se salgono significa che il FIFO si sta riempiendo e potresti aumentare FIFO_DEPTH o chiamare poll() più frequentemente nel loop.

**Esempio d'uso**

```cpp
EspNowManager::setReceiveCallback([](const uint8_t* mac,
                                     const uint8_t* data,
                                     size_t len) {
  // decodifica qui la tua struttura
});

EspNowManager::begin();
EspNowManager::addPeer(mac1);
EspNowManager::addPeer(mac2);

// Nel loop():
EspNowManager::poll();
```
## VERSIONE 1.1.0
**Gestione modulare delle dipendenze**
  - Deploy su github delle librerie condivise
  - version.h con definizioni di versione
  - version_check.h con logica di warning
  - Macro EXPECTED_LIB_VERSION_MAJOR/MINOR nei progetti

**Sottoscrizione dinamica MQTT**
  - Funzione sottoscriviTopics() con array di topic
  - Terminatore nullptr per flessibilità
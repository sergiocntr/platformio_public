Resilient Star - Sistema Domotico IoT v4.5
Benvenuti nella documentazione dell'architettura Enhanced Resilient Star, un ecosistema IoT basato su ESP8266/ESP32 e Raspberry Pi, progettato per garantire la massima stabilità operativa anche in assenza di infrastruttura di rete centrale

* NOTE GENERALI 
1. tutti i file sono listati in `doc/.file_lists` per progetto ,rendendo semplice la navigazone .
2. L' ambiente python e' `/home/sergioc/.platformio/penv/bin/python`
3. L' ambiente platformio e' `/home/sergioc/.platformio/penv/bin/pio`


* 🚀 Visione Architetturale v4.6 (Stable)
L'ecosistema opera come un **Transparent Repeater**. Il Gateway non solo bridgea verso MQTT, ma rilancia istantaneamente ogni pacchetto radio in Broadcast, permettendo aggiornamenti UI globali (Chrono) in tempo reale.

* 🛡️ Strategia di Resilienza v4.6
Tier 1: ESP-NOW (Nativo) con **Smart Handshake**. Discovery in Broadcast -> Pairing Unicast -> Auto-fallback Broadcast in caso di errore.
Tier 2: WiFi + MQTT (Failover). Attivato automaticamente se il Watchdog Heartbeat (2 min) scade.
Tier 3: Broadcast Sociale. Modalità di emergenza P2P in isolamento totale.

--------------------------------------------------------------------------------
* 🛠️ Novità Tecniche
1. **Handshake Intelligente**: I nodi si accoppiano al Gateway solo dopo aver ricevuto un ACK valido. Questo elimina le interferenze da altri dispositivi.
2. **Smart Fallback**: Se un comando Unicast fallisce, il sistema riprova automaticamente in Broadcast resettando il pairing.
3. **Watchdog Heartbeat (33:33)**: Se il broker MQTT è giù, il Gateway emette un'ora "sentinel" (33:33) per mantenere i nodi in radio.
4. **Standard 255.0**: Valore obbligatorio per segnalare sensori guasti.
### SCENARI
* SCENARIO A: MQTT DOWN, Gateway UP
  - Nodi ESP-NOW: dati non arrivano al cloud (MQTT bridge rotto)
  - Gateway emette TYPE_TIME(33:33/???) ogni 60s → nodi restano vivi in Tier 1 ✅
  - Display Nextion mostra "33:33 ???" → sentinel visivo di MQTT DOWN ⚠️
  - Recovery: MQTT torna → Gateway smette il fake HB → primo TYPE_TIME reale ripristina display ✅
  - Nodo NON passa a WiFi finché riceve HB (reale o fake) entro 2 minuti ✅

* SCENARIO B: Gateway DOWN, MQTT UP
  - Boot → Announce senza risposta (900ms) → Fallback WiFi immediato ✅
  - TYPE_TIME arriva via MQTT direttamente da Node-RED ✅
  - Tutto funziona in Tier 2 → sistema stabile ✅
  - Gateway torna → AC_SWITCH_TO_ESPNOW → nodi tornano in radio (Tier 1) ✅

* SCENARIO C: MQTT DOWN + Gateway DOWN
  - Nessun TYPE_TIME di alcun tipo → watchdog scatta dopo 2 min
  - Nodi passano a WiFi (Tier 2) → se anche WiFi fallisce → Social Sleep (Tier 3) ⚠️

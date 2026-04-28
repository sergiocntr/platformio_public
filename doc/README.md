Resilient Star - Sistema Domotico IoT v4.5
Benvenuti nella documentazione dell'architettura Enhanced Resilient Star, un ecosistema IoT basato su ESP8266/ESP32 e Raspberry Pi, progettato per garantire la massima stabilità operativa anche in assenza di infrastruttura di rete centrale

* NOTE GENERALI 
1. tutti i file sono listati in `doc/.file_lists` per progetto ,rendendo semplice la navigazone .
2. L' ambiente python e' `/home/sergioc/.platformio/penv/bin/python`
3. L' ambiente platformio e' `/home/sergioc/.platformio/penv/bin/pio`


* 🚀 Visione Architetturale v4.5  
L'ecosistema si è evoluto da un semplice bridge verso il modello di Transparent Repeater (Ripetitore Trasparente)
. In questa versione, il Gateway non funge solo da ponte verso MQTT, ma rilancia istantaneamente ogni pacchetto radio ricevuto in Broadcast nativo, permettendo a tutti i nodi della rete (come i Chrono) di aggiornarsi in tempo reale senza attendere il broker
.
* 🛡️ Strategia di Resilienza a Tre Livelli
Il sistema opera secondo una gerarchia di trasporto dinamica
:
Tier 1: ESP-NOW (Nativo): Trasporto primario a bassissimo consumo e latenza verso il Gateway
.
Tier 2: WiFi + MQTT (Failover): Attivato automaticamente se il Gateway radio non risponde
.
Tier 3: Broadcast Sociale: Modalità di emergenza P2P in caso di isolamento totale (No Gateway + No WiFi)
.

--------------------------------------------------------------------------------
* 🛠️ Componenti Chiave della v4.5
1. Watchdog Heartbeat Attivo
- Ogni nodo integra un Watchdog di Resilienza basato sulla ricezione dei pacchetti TYPE_TIME (0x08)
.
- Il pacchetto TYPE_TIME (0x08) viene effettuata in prima instanza da MQTT
.
Il gateway percepisce il pacchetto e capisce che MQTT e' operativo.
.
I nodi ESP8266 partono preferibilmente in ESP Now e ricevono il pacchetto TYPE_TIME (0x08) dal gateway.
.
Se il gateway non e' connesso a MQTT, emette autonomamente TYPE_TIME con ora 33:33 ogni 60s (sentinel visivo).
Trigger: Se non viene ricevuto l'heartbeat temporale per più di 2 minuti (120.000 ms) — valore reale in mqttWifi_protocol.cpp: SYNC_TIMEOUT
.
Azione: Il nodo commuta autonomamente il trasporto su WiFi + MQTT per cercare di ristabilire il contatto con il server
.
2. Social Sleep ed Emergenza
In caso di fallimento di tutti i trasporti, i nodi entrano in Social Sleep
:
Sincronizzazione P2P: I nodi inviano i propri dati critici (es. temperature) in broadcast radio verso gli altri nodi alimentati
.
Soft Sleep: Il display Nextion viene spento (thup=1) per risparmiare energia e segnalare visivamente lo stato di emergenza
.
Radio Polling: La radio rimane attiva in finestre ridotte per intercettare il ritorno del Gateway
.
3. Gateway Recovery (Il "Richiamo")
Al riavvio o ripristino del Gateway, viene inviato il comando AC_SWITCH_TO_ESPNOW (0x05) via MQTT
. I nodi in modalità WiFi ricevono questo comando, chiudono la connessione AP e tornano istantaneamente in modalità radio efficiente
.
4. Gestione Errori "Dato Certo"
È obbligatorio per tutti i nodi trasmettere il valore 255.0 in caso di guasto fisico ai sensori (DS18B20 o DHT)
. Questo evita il fenomeno dei "dati congelati" (stale data) sulle interfacce utente
.
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

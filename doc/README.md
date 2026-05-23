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
Tier 1.5: **Exclusive Radio (FORCE_ESPNOW)**. Modalità ultra-low power per sensori a batteria (es. ESPmeteo). Disabilita completamente il fallback WiFi/MQTT per massimizzare la durata della batteria, delegando la resilienza al buffer locale su EEPROM.
Tier 2: WiFi + MQTT (Failover). Attivato automaticamente se il Watchdog Heartbeat (2 min) scade (se non in modalità FORCE_ESPNOW).
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

--------------------------------------------------------------------------------
### 👑 REGOLE D'ORO DELLO SVILUPPO (Protocollo & Ecosistema)
1. **La Regola della Versione (The Version Cascade)**:
   Se modifichi le strutture dei pacchetti, **DEVI obbligatoriamente** incrementare `#define PACKET_VERSION` in `PacketProtocol.h`. Questo incremento innesca una cascata di aggiornamenti *obbligatori*:
   - Aggiornamento dizionari in `tools/packet_generator.py`.
   - Aggiornamento parsing in `nodered_references/config.json`.
   - Aggiornamento del branch di filtro Node-RED ("Filtra solo i pacchetti che vogliamo loggare nel database") e del relativo script PHP backend su Laravel.
2. **Il Sensore Parla, il Cloud Ricorda**:
   Non inviare MAI dati grezzi binari, ACK locali o heartbeat ad alta frequenza al backend Laravel. Node-RED agisce da "traduttore intelligente": aggrega i raw data e fa chiamate API/POST al cloud SOLO quando c'è un dato rilevante o cambia lo stato (es. Health Check).
3. **Immutabilità dei Payload**:
   Una volta che un pacchetto `TYPE_XX` è in produzione, la sua struttura non si tocca. Se serve un campo in più, crea un `TYPE_XX_V2`. Questo evita crash e buffer overflow tra dispositivi aggiornati e non aggiornati.
4. **LOGS**

  * Definizione degli utilizzi dei BIT per ogni tipo di node: vedi `/media/progetti_ext/PROJECT/platformio_public/shared_config/SystemProfiles.h`
    * ESPMETEO: 0b000010001 (0x11)
      - Bit 0: LOG_SERIAL -> Abilita Serial Log
      - Bit 4: FORCE_ESPNOW -> Tier 1.5 Exclusive
      * Senza WiFi, senza UDP Log, senza Nextion, senza PZEM
    * CHRONO: 0b100000010 (0x102)
      - Bit 1: LOG_UDP -> Abilita UDP Log (richiede WiFi)
      - Bit 8: HW_NEXTION -> Presente Nextion (Inibisce Serial Log)
      * Con WiFi, senza Serial Log, senza PZEM
    * GATEWAY: 0b000100000 (0x20)
      - Bit 5: RAD_GATEWAY -> Modalità Gateway (ESP32 Bridge)
      * Con WiFi, senza Serial Log, senza UDP Log, senza Nextion, senza PZEM
  * Serial Test Mode
    * Se vuoi fare una prova in laboratorio su un Chrono con Nextion vedendo anche i log sulla seriale, dovrai usare:
      - SYS_CONFIG = 0b100000101
        * Bit 8: 1 (Nextion)
        * Bit 2: 1 (FORCE Log)
        * Bit 0: 1 (Serial Log)
      * In fase di compilazione, PlatformIO ti mostrerà chiaramente: .../SystemProfiles.h:65: warning: "ATTENZIONE: Log Seriale FORZATO su porta condivisa con Nextion/PZEM (Serial Test Mode)"

      /**
    * @brief Resilient Star - Bitmask System Configuration v2.1
    * 
    * Usa -DSYS_CONFIG=0b<bits> nel platformio.ini
    * 
    * Mappa dei Bit (LEGENDA VISIVA):
    * ---------------------------------------------------------------------------
    * BIT | Valore Bin  | Funzione     | Descrizione
    * ---------------------------------------------------------------------------
    * [0] | 0b000000001 | LOG_SERIAL   | Abilita Serial Log
    * [1] | 0b000000010 | LOG_UDP      | Abilita UDP Log (richiede WiFi)
    * [2] | 0b000000100 | LOG_FORCE    | Forza Serial Log anche con conflitti HW
    * [3] | N/A         | N/A          | N/A
    * [4] | 0b000010000 | RAD_FORCE_NOW| Tier 1.5: Exclusive ESP-NOW (No WiFi fallback)
    * [5] | 0b000100000 | RAD_GATEWAY  | Modalità Gateway (ESP32 Bridge)
    * [6] | N/A         | N/A          | N/A
    * [7] | N/A         | N/A          | N/A
    * [8] | 0b100000000 | HW_NEXTION   | Presente Nextion (Inibisce Serial Log)
    * [8] | 0b100000000 | HW_PZEM      | Presente PZEM (Inibisce Serial Log)
    * ---------------------------------------------------------------------------
    * 
    * ESEMPI PRATICI:
    * - ESPmeteo (Radio Exclusive + Serial Log):
    *   SYS_CONFIG = 0b000010001 (Radio bit 4 + Serial bit 0)
    * 
    * - Chrono (Nextion + UDP Log):
    *   SYS_CONFIG = 0b100000010 (Nextion bit 8 + UDP bit 1)
    */



### 👑 REGOLA D'ORO #5: La Cittadinanza Duale (Scalability Rule)

Per garantire la convivenza tra attuatori reattivi e centinaia di sensori a batteria, l'ecosistema Resilient Star divide i dispositivi in due classi gerarchiche con diverse giurisdizioni e regole di ingaggio.

#### I Cittadini Permanenti (Peer Stabili)

   - **Chi sono**: Nodi alimentati da rete elettrica o attuatori critici (es. Caldaia,  Chrono, EnergyMain)

   - **Competenze**: Esecuzione di comandi in tempo reale, gestione di interfacce UI (Nextion), monitoraggio continuo del carico elettrico
   
   - **Giurisdizione**: Tabella Peer hardware del Gateway (limite hardware di 20 slot)

   - Hanno diritto a comunicazioni Unicast garantite dal protocollo 802.11

	 **Regole di Ingaggio**:

- **Identificazione**: Devono inviare obbligatoriamente un TYPE_ANNOUNCE all'avvio per essere "imparati" dal Gateway tramite learnPeer()

- **Sincronizzazione**: Devono rispondere all'Heartbeat TYPE_TIME (Watchdog) per mantenere la residenza nella tabella

- **Comandi**: Ricevono comandi Unicast diretti e devono rispondere con un ACK binario

#### I Cittadini Nomadi (One-Shot / Visitors)

 - **Chi sono**: Sensori a batteria a bassissimo consumo e sensori "mordi e fuggi" (es. ESP32C3_Piante, ESPMeteo)

 - **Competenze**: Telemetria ambientale periodica (umidità suolo, dati meteo)

 - **Giurisdizione**: Trasmissione "eterea" (Broadcast). Non occupano slot nella tabella Peer hardware del Gateway, permettendo una scalabilità virtualmente infinita

   **Regole di Ingaggio:**

 - **Identificazione**: **Non** effettuano l'handshake di ANNOUNCE per risparmiare energia e non saturare la memoria del Gateway

 - **Trasmissione**: "Urlano" i dati in Broadcast (FF:FF:FF:FF:FF:FF) e tornano immediatamente in Deep Sleep

 - **Ricezione**: Accettano solo ACK o messaggi collettivi inviati in Broadcast dal Gateway.

 - **Limitazione**: Non possono ricevere comandi Unicast poiché non sono peer registrati e trascorrono il 99% del tempo spenti

 - **Health Check**: Riportano il proprio stato di salute simulando un Heartbeat tramite un pacchetto TYPE_ACK inviato subito dopo i dati (cmdEcho = TYPE_TIME)

.

#### Il Ruolo del Gateway (Il Doganiere)

**Il Gateway agisce come arbitro della cittadinanza**

1. In fase di ricezione radio (onEspNowRecv), identifica il tipo di pacchetto:

2. Se il pacchetto è One-Shot (TYPE_METEO, TYPE_DATA o HealthAck), il Gateway processa il dato e lo inoltra a MQTT, ma salta la funzione learnPeer(), proteggendo la tabella hardware

3. Se il pacchetto è un Announce, il Gateway registra ufficialmente il nodo, garantendogli i privilegi della cittadinanza permanente (Unicast e Heartbeat attivo)

--------------------------------------------------------------------------------
* 📝 TODO LIST (Miglioramenti Futuri)
- **Integrità Dati (v5.0)**: Sostituire il checksum XOR con un **CRC8** o **CRC16** per garantire una immunità assoluta contro i burst error e inversioni di byte sul canale radio ESP-NOW.
- **Auto-segnalazione Fault**: Fare in modo che se un nodo rileva la rottura di un sensore *tra* un Heartbeat e l'altro, emetta proattivamente un `TYPE_ACK` di fault, senza aspettare l'interrogazione del Gateway.

* 💡  Consigli per migliorare i log (Best Practices):
Se vuoi rendere il debug ancora più immediato in futuro, potresti aggiungere questi piccoli dettagli:

1. **Log degli MQTT Inbound "ignoti"**: Nel Gateway, la funzione onMqttMessage era un po' troppo "silenziosa" quando non capiva un messaggio. Ho aggiunto dei LOG_WARN nel fix, ma in generale è bene loggare sempre quando un messaggio MQTT viene scartato e perché (es: "Scartato: deviceID non trovato in tabella").

2. **Includere il Type testuale**: Invece di loggare solo Type: 0x03, potresti usare una funzione di traduzione (se lo spazio flash lo permette) per scrivere Type: 0x03 (METEO). Aiuta a leggere i log molto più velocemente senza consultare il PacketProtocol.h.

3. **Contatore di sequenza**: Nel pacchetto TYPE_METEO hai già un counter. Sarebbe utile visualizzarlo sempre nei log: [RX] Meteo da 0xD0 | Seq: 42. Se vedi saltare i numeri, capisci subito se c'è una perdita di pacchetti radio prima ancora che Node-RED se ne accorga.

4. **Log del tempo di volo (Round Trip Time)**: Sulla sonda meteo, loggare quanto tempo è passato tra l'invio e la ricezione dell'ACK: [ACK] Ricevuto in 150ms. Se vedi questo tempo alzarsi sopra i 500ms, significa che il bridge MQTT/Gateway sta diventando lento.
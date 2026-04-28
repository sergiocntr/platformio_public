

# Protocollo ESP-NOW v3.0 (Resilient Star)
Specifiche tecniche e "Must-Have" per i nodi foglia (Chrono, Caldaia, Sensori).

Questo documento riassume le regole d'oro implementate durante la migrazione del 19 Aprile 2026 per garantire la stabilità della rete radio.

## 1. Architettura di Rete
La rete opera in modalità **Resilient Star**:
- **Gateway**: Unico nodo connesso sia al WiFi (STA) che alla Radio (AP). Si occupa di rilanciare in broadcast radio tutto ciò che arriva da MQTT.
- **Nodi Foglia**: Operano esclusivamente in ESP-NOW. Non sono connessi al WiFi per ridurre i consumi e aumentare la reattività.

## 2. Le 5 Regole d'Oro del Setup (Firmware)

### I. Ordine di inizializzazione (LA REGOLA D'ORO)
Per abilitare il broadcast e la stabilità radio, l'ordine **deve** essere:
1. `esp_now_init()`
2. Registrazione Callbacks (`esp_now_register_recv_cb`)
3. Impostazione Canale (`wifi_set_channel` o `esp_wifi_set_channel`)
4. Registrazione Peer (`FF:FF:FF:FF:FF:FF`)

Se il canale viene toccato prima dell'init (vecchia scuola), il broadcast fallirà silenziosamente.

### II. Handshake di Scoperta (Manshake)
I nodi non conoscono il MAC address "reale" del Gateway all'avvio:
1. Il nodo invia un `ANNOUNCE` (Type 0x01) presentandosi col proprio reale Device ID (es. `0x10`) e allegando nei bytes 7 e 8 la propria `versione` di firmware passata alla libreria in compilazione. (N.B. Rimosso definitivamente l'uso dell'hardcoded ID temporaneo `0xFE`).
2. Il Gateway, ricevuto il pacchetto, memorizza il MAC del nodo e risponde con un `TYPE_ACK` (0x00) inviato OBBLIGATORIAMENTE in **UNICAST** al MAC mittente (e NON in Broadcast). Questo approccio garantisce la consegna a livello 802.11 e previene la perdita dell'ACK lato nodo (causa dei ripieghi imprevisti verso MQTT).
3. Il nodo "impara" il MAC del Gateway dal pacchetto ACK ricebuto e si aggancia stabilmente per tutta la sessione operativa.

### III. Canale Radio
Tutti i nodi devono operare sullo stesso canale del Gateway (attualmente **Canale 12**). Se il router WiFi cambia canale, il Gateway lo segue e i nodi devono essere aggiornati o configurati per scansionare.

### IV. Peer Registration
- Al setup, aggiungere sempre il peer `FF:FF:FF:FF:FF:FF` (Broadcast).
- Senza questo peer, l'ESP8266 ignorerà i messaggi di sistema (Ora, Meteo, Comandi collettivi) rilanciati dal Gateway.

### V. Identità (Device ID)
Ogni nodo deve avere un ID univoco definito in `devices.h`. Il nodo deve sempre identificarsi nel pacchetto radio (campo `deviceID`) affinché il Gateway possa instradare correttamente i messaggi verso MQTT.

## 3. Struttura del Pacchetto (v3.0)
Ogni frame deve seguire rigorosamente questo header (5 byte):
1. **Magic**: `0xAA`
2. **Version**: `0x03` (Fondamentale: le versioni precedenti non verranno processate)
3. **Type**: Tipo di dato (0x01=Announce, 0x03=DHT, 0x08=Time, ecc.)
4. **Len LSB**: Lunghezza payload (parte bassa)
5. **Len MSB**: Lunghezza payload (parte alta)
6. **Payload**: Dati binari (struct)
7. **Checksum**: XOR di TUTTI i byte precedenti (Header + Payload).

## 4. Diagnostica sul Gateway
In caso di problemi, il Gateway riporta due tipi di conferme nei log:
- `[TX-ACK] Consegnato`: La radio del nodo ha ricevuto il pacchetto e ha mandato un ACK hardware. La radio funziona.
- `[RX]`: Il Gateway ha ricevuto un pacchetto dal nodo. Se vedi RX ma non vedi aggiornamenti sul display, il problema è nel software del nodo (parsing o XOR errato).

## 5. Resilienza e Fallback (Auto-Healing)
La libreria `mqttWifi` implementa un meccanismo di protezione per evitare che il nodo diventi isolato se il Gateway Radio non è raggiungibile:

1. **Tentativo ESP-NOW**: All'avvio (`setupCompleto`), il nodo invia 3 messaggi di `ANNOUNCE`.
2. **Timeout**: Se non riceve risposta (ACK/Echo) entro ~1 secondo, assume che il Gateway radio sia offline o fuori portata.
3. **Fallback WiFi**: Il sistema commuta automaticamente il trasporto su `MqttTransportType::WIFI`. Viene attivata la connessione WiFi standard all'Access Point del router e il protocollo comunica direttamente col broker MQTT.
4. **Strategia Notturna (v3.2)**: Se falliscono sia ESP-NOW che WiFi (es. Router e Gateway spenti), il nodo entra in **Deep Sleep per 5 minuti**. Questo ciclo si ripete finché non viene ripristinata l'infrastruttura, garantendo risparmio energetico e protezione della radio.
5. **Ripristino**: Il nodo continuerà in modalità WiFi fino al prossimo riavvio o finché la logica di `gestisciConnessione()` non rileva la disponibilità del trasporto radio.

## 6. Smart Radio Handoff (v3.3)
Per risolvere il problema dei nodi che rimangono "appesi" al WiFi quando la radio sarebbe disponibile, è stato introdotto il comando di **Handoff Automatico**:

1. **Tentativo di Contatto**: Un nodo configurato per entrambi i trasporti (default) si presenta al Gateway inviando un `ANNOUNCE` via ESP-NOW.
2. **Invito allo Switch**: Se il Gateway riceve l'announce via radio, risponde con un `TYPE_ACK` avente `status = AC_SWITCH_TO_ESPNOW` (0x05).
3. **Esecuzione**: La libreria `mqttWifi` del nodo, ricevendo questo stato:
   - Commuta il trasporto interno a `MqttTransportType::ESPNOW`.
   - Esegue `WiFi.disconnect()` per liberare la radio.
   - Entra in modalità "Solo Radio" definitiva fino al prossimo reboot o fallimento radio.

Questo meccanismo garantisce che la rete converga verso ESP-NOW il più velocemente possibile, riducendo l'inquinamento elettromagnetico e il carico sul router WiFi.

## 7. Appunti di Sviluppo e Troubleshooting Rapido
1. **Compilazione dei Nodi e Ambiente CLI**: Spesso macro e variabili di build (es. `versione`) falliscono se compiliate al di fuori del path corretto di sistema o con eseguibili disallineati. Validare sempre con il comando `pio` assoluto corretto:
   ```bash
   # Esempio per NodeMCU ESP8266 (es. Chrono, nodecaldaia, enemain):
   /home/sergioc/.platformio/penv/bin/pio run --environment nodemcuv2
   
   # Esempio per Gateway ESP32:
   /home/sergioc/.platformio/penv/bin/pio run --environment lolin32_lite
   ```
3. **Gestione Errori Sensore (Dato Certo)**: Quando un sensore DS18B20 o DHT fallisce, il nodo trasmette **255.0**. Questo permette ai display di mostrare "Error" o "---" invece di mantenere l'ultima lettura valida.

---
*Ultimo aggiornamento: 24/04/2026 - Rimozione handshake 0xFE, Fix ACK Unicast per ESP-NOW, Istruzioni sviluppo pio.*

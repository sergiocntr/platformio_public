# Protocollo ESP-NOW v4.0 (Enhanced Resilient Star)
*Data di rilascio: 26 Aprile 2026*

Questo documento sancisce l'evoluzione del protocollo radio verso una gestione nativa del **Broadcast** e l'integrazione trasparente di sensori MQTT e Radio.

## 1. Novità Principali (v4.0 vs v3.x)

### I. Broadcast Nativo (Radio-to-Radio)
Il Gateway agisce ora come un **Ripetitore Trasparente**. Ogni pacchetto ricevuto  via radio (NB: non deve essere TYPE_COMMAND o TYPE_TIME) viene immediatamente rispedito in broadcast (`FF:FF:FF:FF:FF:FF`). (perche' non lo trasmette gia' in broadcast il nodo?)
*   **Vantaggio**: Tutti i display (Chrono) ricevono gli ACK di conferma della esecuzione dei TYPE_COMMAND (che vengono trasmessi sempre un broadcast es. dai nodi Caldaia) istantaneamente, permettendo un aggiornamento globale delle icone senza polling.

### II. Heartbeat Unicast (Ping Ora)
Il pacchetto `TYPE_TIME` (0x08) continua a viaggiare in **Unicast Unroll**. 
*   **Scopo**: Fungere da "Keep-Alive". Se un nodo non risponde all'ora per 3 cicli, viene marcato come *Offline* dal Gateway. Questo evita di inquinare la radio con broadcast di dati se i nodi sono spenti.

### III. Bridge Selettivo MQTT (RPi3 Support)
Il Gateway si iscrive ora al topic `espNowBridge/buffer`. Se riceve un pacchetto da un dispositivo puramente MQTT (es. RPi3 via WiFi), lo rilancia via Radio.
*   **Loop Protection**: Il Gateway inoltra alla radio solo i pacchetti i cui `deviceID` non sono marcati come `isEspNow`, evitando ridondanze infinite.

### IV. Segnale di Errore "Certo" (255.0)
È obbligatorio per i nodi trasmettere il valore **255.0** in caso di guasto ai sensori (DS18B20/DHT).
*   **Stale Data Prevention**: Mai più valori "congelati" sul display se un sensore muore.

## 2. Implementazione della "Regola d'Oro"
Per la stabilità dell'ESP8266 sul broadcast, l'ordine di avvio non è negoziabile:
1.  `esp_now_init()`
2.  `esp_now_register_recv_cb()`
3.  `wifi_set_channel(12)`
4.  `esp_now_add_peer(broadcastAddress)`

## 3. Comandi e ACK
*   **Comandi**: Sempre inviati dal nodo verso il Gateway (Unicast).
*   **ACK**: Prodotti dal nodo destinatario e rilanciati dal Gateway in Broadcast.
*   **Display**: I display devono aggiornare le icone su RICEZIONE ACK, indipendentemente da chi ha originato il comando.

---
*Aggiornato con orgoglio il 26 Aprile 2026 - Il giorno del Vero Broadcast.*

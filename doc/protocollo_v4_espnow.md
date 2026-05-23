# Protocollo Resilient Star v4.6 (Handshake Intelligente)
*Ultimo aggiornamento: 28 Aprile 2026 - Il giorno della Resilienza Unicast*

Questo documento descrive la logica di comunicazione definitiva per la rete radio Resilient Star.

## 1. Handshake Intelligente (Discovery via Broadcast)
Per garantire la stabilità dell'ESP8266 ed evitare collisioni, i nodi seguono un pattern di accoppiamento dinamico:

1.  **Boot (Discovery)**: Il nodo parte con `g_gateway_paired = false`. 
2.  **Primo Invio**: Tutti i comandi (`TYPE_COMMAND`) o annunci (`ANNOUNCE`) vengono inviati in **Broadcast** (`FF:FF:FF:FF:FF:FF`).
3.  **Accoppiamento**: Se il Gateway risponde con un `TYPE_ACK` che contiene il `deviceID` del nodo o del comando, il nodo salva il MAC del Gateway e imposta `g_gateway_paired = true`.
4.  **Operatività Unicast**: Da questo momento, le comunicazioni verso il Gateway avvengono in **Unicast** (più veloce ed efficiente).

## 2. Smart Fallback (Auto-Guarigione)
Se una comunicazione Unicast fallisce (non riceve un ACK a livello di protocollo entro il timeout), il sistema:
*   Reset delle impostazioni di pairing (`g_gateway_paired = false`).
*   Esegue immediatamente un **Retry in Broadcast**.
*   Questo garantisce che il comando arrivi sempre a destinazione, anche se il Gateway ha cambiato canale o ha resettato la sua tabella dei peer.

## 3. La "Regola d'Oro" dell'Inizializzazione
Sull'ESP8266, per ricevere i broadcast senza perdite, l'ordine è tassativo:
1.  `WiFi.mode(WIFI_STA)` + `WiFi.disconnect()`
2.  `esp_now_init()`
3.  `esp_now_set_self_role(ESP_NOW_ROLE_COMBO)`
4.  `esp_now_register_recv_cb(onInternalEspNowRx)`
5.  **Solo ora** aggiungere i peer (Broadcast e Gateway).

## 4. Watchdog e Heartbeat
*   **TYPE_TIME (0x08)**: È il battito cardiaco della rete.
*   **Timeout**: 2 minuti (120s). Se scadono, il nodo commuta in **WiFi + MQTT**.
*   **MQTT Down**: Se il Gateway non sente MQTT, emette un "fake heartbeat" (33:33) per mantenere i nodi in radio ed evitare passaggi inutili al WiFi.

## 5. Standard Sensori
*   **Valore 255.0**: Segnala sensore guasto o scollegato. Obbligatorio per tutti i nodi.

## 6. Note di sviluppo
### Riepilogo Interventi - migliorie Gateway
1.  **Bridge Trasparente**: Abbiamo abilitato l'inoltro Unicast per tutti i tipi di pacchetti binari da MQTT a Radio (non solo i comandi).
2.  **Tracking Intelligente**: Abbiamo impedito che i pacchetti di ritorno (ACK/Comandi) sovrascrivessero erroneamente lo stato dei nodi radio nella tabella LRU del Gateway.
3.  **Parsing Robusto**: Abbiamo aggiunto il supporto per gli ACK in formato JSON e reso sicura la gestione dei buffer MQTT non terminati.
4.  **Peer Management**: Abbiamo semplificato la gestione dei peer ESP-NOW per evitare errori di registrazione duplicata.

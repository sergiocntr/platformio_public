# Regole del Broadcast e Resilienza (v4.5)
Documento di riferimento per la gestione dei messaggi collettivi e Heartbeat nella rete "Resilient Star".

## 1. Strategia di Invio Radio (Gateway)
Per ottimizzare la radio ed evitare collisioni, il Gateway adotta una strategia mista:

| Tipo Pacchetto | Metodo di Invio | Scopo |
| :--- | :--- | :--- |
| **TYPE_TIME** (0x08) | **Unicast Unroll** | Fungere da **Heartbeat**. Ogni invio Unicast riuscito resetta il Watchdog del nodo ricevente. |
| **TYPE_COMMAND** (Switch) | **Broadcast (MQTT)** | Il comando `AC_SWITCH_TO_ESPNOW` (0x05) è inviato via MQTT per richiamare i nodi dal WiFi alla Radio. |
| **TYPE_ACK** (da MQTT/lib) | **Broadcast** | Permettere a tutti i display (Chrono) di aggiornare le icone degli stati in tempo reale. |
| **Rebroadcast** dati ricevuti | **Broadcast** | Aggiornare istantaneamente tutta la rete sui cambiamenti di stato. |

## 2. Watchdog Heartbeat (Device Side)
La libreria `mqttWifi` integra un watchdog attivo basato sulla ricezione dei pacchetti `TYPE_TIME`.
*   **Frequenza**: Il Gateway invia il tempo periodicamente.
*   **Timeout**: 300.000 ms (5 minuti).
*   **Failover**: Se il timer scade, il nodo invoca `setMqttTransport(MqttTransportType::WIFI)`.

## 3. Social Sleep e Resilienza Estrema
In caso di fallimento di tutti i trasporti (Gateway assente AND WiFi/Broker assente), i nodi entrano in "Social Sleep":
1.  **Sincronizzazione P2P**: Il nodo invia i propri dati in Broadcast radio `FF:FF:FF:FF:FF:FF`. Gli altri nodi (se alimentati) catturano questi dati per mantenere le informazioni minime (es. temperatura esterna sul display).
2.  **Soft Sleep**: Il display Nextion viene spento (`thup=1`) per risparmiare energia e segnalare visivamente lo stato di emergenza.
3.  **Radio Polling**: La radio rimane attiva solo per brevi finestre di ascolto per intercettare l'eventuale ritorno del Gateway.

## 4. Nuova Struttura Libreria (mqttWifi)
Il refactoring ha separato le responsabilità per migliorare la manutenibilità:
*   **`mqttWifi.cpp`**: Gestione connessione fisica (WiFi, MQTT Loop, Sleep).
*   **`mqttWifi_protocol.cpp`**: Logica del protocollo binario, gestione ACK, Dispatcher e Watchdog.
*   **`mqttWifi_transport.cpp`**: Astrazione tra ESP-NOW e WiFi.

## 5. Segnalazione Guasti Sensori
Per evitare di mostrare valori "congelati" (stale data):
*   Se un sensore fallisce la lettura: trasmettere il valore **255.0**.
*   Interpretazione: `255.0` = "Sensore Guasto" o "Dato non disponibile".

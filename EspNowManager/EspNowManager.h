#pragma once
#include <Arduino.h>

#if defined(ESP32)
  #include <WiFi.h>
  #include <esp_now.h>
#elif defined(ESP8266)
  #include <espnow.h>
#else
  #error "EspNowManager: piattaforma non supportata (richiede ESP32 o ESP8266)"
#endif

/**
 * @namespace EspNowManager
 * @brief Transport layer generico per ESP-NOW con gestione peer multipli
 *        e buffer FIFO thread-safe per la ricezione.
 *
 * Flusso tipico:
 * @code
 *   // 1. Registra le callback PRIMA di begin()
 *   EspNowManager::setReceiveCallback(
 *     [](const uint8_t* mac, const uint8_t* data, size_t len) {
 *       // chiamata da poll(), nel loop principale
 *     }
 *   );
 *
 *   // 2. Inizializza
 *   EspNowManager::begin();
 *
 *   // 3. Aggiungi peer
 *   EspNowManager::addPeer(peerMac);
 *
 *   // Nel loop():
 *   EspNowManager::poll();   // svuota il FIFO e chiama la callback
 *
 *   // Per inviare:
 *   EspNowManager::send(peerMac, buf, len);
 * @endcode
 */
namespace EspNowManager {

// ---------------------------------------------------------------------------
// Costanti configurabili
// ---------------------------------------------------------------------------

/** Numero massimo di peer registrabili (limite hardware ESP-NOW: 20). */
static constexpr uint8_t  MAX_PEERS       = 20;

/** Numero di slot nel buffer FIFO di ricezione. */
static constexpr uint8_t  FIFO_DEPTH      = 8;

/** Dimensione massima (byte) di un singolo messaggio nel FIFO. */
static constexpr uint8_t  MAX_PACKET_SIZE = 128;

/** Dimensione massima consentita da ESP-NOW per payload. */
static constexpr uint8_t  ESPNOW_MAX_LEN  = 250;

// ---------------------------------------------------------------------------
// Tipi pubblici
// ---------------------------------------------------------------------------

/**
 * @brief Callback invocata da poll() per ogni messaggio nel FIFO.
 *
 * @param mac   Indirizzo MAC del mittente (6 byte).
 * @param data  Buffer dati ricevuto.
 * @param len   Lunghezza effettiva dei dati (<= MAX_PACKET_SIZE).
 */
using ReceiveCallback = void (*)(const uint8_t* mac,
                                 const uint8_t* data,
                                 size_t         len);

/**
 * @brief Callback opzionale invocata dopo ogni invio (dal contesto ISR).
 *        Mantenere il codice al minimo indispensabile.
 *
 * @param mac      Indirizzo MAC del destinatario.
 * @param success  true se la consegna è avvenuta con successo.
 */
using SendCallback = void (*)(const uint8_t* mac, bool success);

/**
 * @brief Informazioni su un peer registrato.
 */
struct PeerInfo {
  uint8_t mac[6];   ///< Indirizzo MAC.
  uint8_t channel;  ///< Canale Wi-Fi (0 = corrente).
  bool    active;   ///< true se lo slot è occupato.
};

/**
 * @brief Statistiche operative (utili per debug e monitoraggio).
 */
struct Stats {
  uint32_t rxTotal;      ///< Pacchetti ricevuti totali.
  uint32_t rxDropped;    ///< Pacchetti scartati (FIFO pieno o dimensione).
  uint32_t txTotal;      ///< Invii richiesti e presi in carico dallo stack.
  uint32_t txFailed;     ///< Invii falliti (errore stack o parametri).
  uint32_t txDelivered;  ///< Consegne confermate via SendCallback.
};

// ---------------------------------------------------------------------------
// Configurazione
// ---------------------------------------------------------------------------

/**
 * @brief Registra la callback per la ricezione (chiamata da poll()).
 *        Deve essere impostata prima di begin().
 */
void setReceiveCallback(ReceiveCallback cb);

/**
 * @brief Registra la callback opzionale post-invio (contesto ISR).
 */
void setSendCallback(SendCallback cb);

// ---------------------------------------------------------------------------
// Gestione peer
// ---------------------------------------------------------------------------

/**
 * @brief Aggiunge un peer alla lista e allo stack ESP-NOW.
 *
 * @param mac      Indirizzo MAC del peer (6 byte).
 * @param channel  Canale Wi-Fi (0 = canale corrente).
 * @return true se aggiunto con successo o già presente.
 */
bool addPeer(const uint8_t* mac, uint8_t channel = 0);

/**
 * @brief Rimuove un peer dalla lista e dallo stack ESP-NOW.
 *
 * @param mac  Indirizzo MAC del peer da rimuovere.
 * @return true se rimosso con successo.
 */
bool removePeer(const uint8_t* mac);

/**
 * @brief Verifica se un peer è già registrato.
 */
bool hasPeer(const uint8_t* mac);

/**
 * @brief Restituisce il numero di peer attualmente registrati.
 */
uint8_t peerCount();

/**
 * @brief Popola un array con le informazioni sui peer attivi.
 *
 * @param out      Array di PeerInfo da riempire (dimensione >= MAX_PEERS).
 * @param outCount Numero di peer scritti nell'array.
 */
void getPeers(PeerInfo* out, uint8_t& outCount);

// ---------------------------------------------------------------------------
// Ciclo di vita
// ---------------------------------------------------------------------------

/**
 * @brief Inizializza ESP-NOW e registra le callback interne.
 *        Il Wi-Fi deve essere già attivo prima di chiamare begin().
 *
 * @return true se l'inizializzazione è avvenuta con successo.
 */
bool begin();

/**
 * @brief Deinizializza ESP-NOW, svuota il FIFO e rimuove tutti i peer.
 */
void end();

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------

/**
 * @brief Elabora tutti i messaggi pendenti nel FIFO e invoca la callback.
 *        Da chiamare nel loop() principale.
 *
 * @return Numero di messaggi elaborati in questa chiamata.
 */
uint8_t poll();

// ---------------------------------------------------------------------------
// Trasmissione
// ---------------------------------------------------------------------------

/**
 * @brief Invia un buffer di byte raw a un peer registrato.
 *
 * @param mac   Indirizzo MAC del destinatario (deve essere peer registrato).
 * @param data  Buffer da inviare.
 * @param len   Lunghezza in byte (max ESPNOW_MAX_LEN = 250).
 * @return true se l'invio è stato preso in carico dallo stack.
 */
bool send(const uint8_t* mac, const uint8_t* data, size_t len);

// ---------------------------------------------------------------------------
// Diagnostica
// ---------------------------------------------------------------------------

/**
 * @brief Restituisce l'esito dell'ultimo invio completato.
 */
bool lastSendSucceeded();

/**
 * @brief Restituisce una copia delle statistiche operative correnti.
 */
Stats getStats();

/**
 * @brief Azzera le statistiche operative.
 */
void resetStats();

} // namespace EspNowManager
#include "PacketProtocol.h"
#include <string.h>

/**
 * @file PacketProtocol.cpp
 * @brief Implementation for the standardized communication buffer protocol.
 */

// ── XOR checksum ─────────────────────────────────────────────────────────────
uint8_t pp_calculateXOR(const uint8_t *data, size_t length) {
    uint8_t result = 0;
    for (size_t i = 0; i < length; i++) {
        result ^= data[i];
    }
    return result;
}

// ── Frame construction ────────────────────────────────────────────────────────
size_t pp_buildPacket(uint8_t type,
                      const uint8_t *payload, uint16_t payloadLen,
                      uint8_t *outBuffer) {
    if (outBuffer == NULL) return 0;

    // 1. Header (5 bytes)
    outBuffer[0] = PACKET_MAGIC;
    outBuffer[1] = PACKET_VERSION;
    outBuffer[2] = type;
    outBuffer[3] = (uint8_t)(payloadLen & 0xFF);         // length LSB
    outBuffer[4] = (uint8_t)((payloadLen >> 8) & 0xFF);  // length MSB

    // 2. Payload
    if (payload != NULL && payloadLen > 0) {
        memcpy(outBuffer + HEADER_SIZE, payload, payloadLen);
    }

    // 3. XOR checksum over header + payload
    uint8_t checksum = pp_calculateXOR(outBuffer, HEADER_SIZE + payloadLen);
    outBuffer[HEADER_SIZE + payloadLen] = checksum;

    return HEADER_SIZE + payloadLen + 1;
}

// ── Frame validation ──────────────────────────────────────────────────────────
int pp_validatePacket(const uint8_t *buffer, size_t bufferSize) {
    if (buffer == NULL || bufferSize < PACKET_MIN_SIZE) return -1;

    // 1. Magic word
    if (buffer[0] != PACKET_MAGIC) return -2;

    // 2. Declared payload length
    uint16_t payloadLen = (uint16_t)buffer[3] | ((uint16_t)buffer[4] << 8);

    // 3. Total size must match: HEADER(5) + payload + checksum(1)
    if (bufferSize != (size_t)(HEADER_SIZE + payloadLen + 1)) return -3;

    // 4. XOR checksum (excludes the checksum byte itself)
    uint8_t expected = pp_calculateXOR(buffer, HEADER_SIZE + payloadLen);
    if (buffer[HEADER_SIZE + payloadLen] != expected) return -4;

    return 0;
}

// ── Frame parsing (validate + decode) ────────────────────────────────────────
int pp_parsePacket(const uint8_t *buffer, size_t bufferSize, ParsedPacket *out) {
    if (out == NULL) return -5;

    int rc = pp_validatePacket(buffer, bufferSize);
    if (rc != 0) return rc;

    out->header.magic         = buffer[0];
    out->header.version       = buffer[1];
    out->header.type          = buffer[2];
    out->header.payloadLength = (uint16_t)buffer[3] | ((uint16_t)buffer[4] << 8);

    // Zero-copy: point directly into the caller's buffer
    out->payload = (out->header.payloadLength > 0)
                   ? (buffer + HEADER_SIZE)
                   : NULL;

    return 0;
}
#include "log_lib.h"
#include <WiFiUdp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef DEBUG_UDP_LOG
WiFiUDP udpLog;
IPAddress remoteIP;
bool udpInitialized = false;
#ifdef DEVICE_ID
char log_device_name[32] = DEVICE_ID;
#else
char log_device_name[32] = {0};
#endif

void udpLogBegin() { 
    // Inizializza il socket UDP su una porta locale casuale per evitare conflitti
    if (udpLog.begin(2222 + random(1000))) {
        udpInitialized = true;
    }
    remoteIP.fromString(UDP_LOG_IP);
}

void logSetDeviceName(const char* name) {
    if (name) {
        strncpy(log_device_name, name, sizeof(log_device_name) - 1);
        log_device_name[sizeof(log_device_name) - 1] = '\0';
    }
}

void udpLogSend(const char *msg) {
    if (!udpInitialized || WiFi.status() != WL_CONNECTED) return;

    // Usiamo l'oggetto IPAddress già convertito per maggiore velocità e affidabilità
    if (udpLog.beginPacket(remoteIP, UDP_LOG_PORT)) {
        udpLog.write((const uint8_t *)msg, strlen(msg));
        udpLog.endPacket();
    }
}

void udpLogSend_f(const char *fmt, ...) {
    if (WiFi.status() != WL_CONNECTED) return;
  
    char buf[256];
    int pos = 0;

    if (log_device_name[0] != '\0') {
        pos = snprintf(buf, sizeof(buf), "[%s] ", log_device_name);
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf + pos, sizeof(buf) - pos, fmt, args);
    va_end(args);
    
    udpLogSend(buf);
}
#endif

void logSerialBegin(uint32_t baud) {
#ifdef DEBUG_SERIAL_LOG
    LOG_SERIAL_PORT.begin(baud);
#endif
}

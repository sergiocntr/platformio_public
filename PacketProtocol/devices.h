#ifndef DEVICES_H
#define DEVICES_H

/**
 * @file devices.h
 * @brief Device and sensor registry.
 *
 * DeviceID encoding — 1 byte, two nibbles:
 *
 *   0xZY
 *     Z = physical node  (the board)
 *     Y = sensor on that node (0 = first/only sensor)
 *
 * Examples:
 *   0x50 = node 5, sensor 0 (DS18B20 on caminetto board)
 *   0x51 = node 5, sensor 1 (Thermocouple K on same board)
 *   0x10 = node 1, sensor 0 (DHT22 on chrono board)
 *   0x11 = node 1, sensor 1 (second DHT22 on same chrono board, future)
 *
 * PacketType describes HOW to deserialise the payload  → PacketProtocol.h
 * deviceID   describes WHO sent it (node + sensor)
 *
 * Adding a new sensor on an existing node  → new Y on same Z
 * Adding a new node                        → new Z, Y starts at 0
 *
 * ┌────────┬───────────────────────┬──────────────┬──────────────┬────────────────────────────────────────┐
 * │  ID    │ Name                  │ Location     │ Hardware     │ Notes                                  │
 * ├────────┼───────────────────────┼──────────────┼──────────────┼────────────────────────────────────────┤
 * │ 0x01   │ CTRL_MAIN_CONTROL     │ Salotto      │ ESP8266      │ Toglie tensione ai servizi della casa  │
 * │ 0x02   │ CTRL_AIR_COND         │ Corridoio    │ ESP8266 fut. │ Payload: on/off, caldo/freddo, temp    │
 * │ 0x03   │ CTRL_HEAT_SYSTEM      │ Esterno      │ ESP8266      │ Controllo riscaldamento                │
 * │ 0x04   │ CTRL_HOT_WATER        │ Esterno      │ ESP8266      │ Controllo acqua calda sanitaria        │
 * │ 0x05   │ CTRL_TUYA_TENDE       │ Casa         │ Node-RED     │ Tuya blind controller bridge           │
 * ├────────┼───────────────────────┼──────────────┼──────────────┼────────────────────────────────────────┤
 * │ 0x10   │ DEV_CHRONO_DHT_1      │ Salotto      │ ESP/CHRONO   │ DHT22 – sul dispositivo                │
 * │ 0x20   │ DEV_RPI_DHT_1         │ Camera grande│ Raspberry 3  │ DHT22 – sul dispositivo                │
 * │ 0x30   │ DEV_BAGNO_DS18B20     │ Bagno        │ ESP8266      │ DS18B20 – Display Nextion              │
 * │ 0x40   │ DEV_ENERGYMAIN_PZEM   │ Salotto      │ ESP8266      │ PZEM – quadro elettrico                │
 * │ 0x50   │ DEV_CAMINETTO_DS18B20 │ Salotto      │ ESP8266      │ DS18B20 – caminetto                    │
 * │ 0x51   │ DEV_CAMINETTO_SONDA_K │ Salotto      │ ESP8266      │ Termocoppia K – stesso nodo caminetto  │
 * │ 0x70   │ DEV_ESP_CAMERA        │ Camera pic.  │ ESP8266      │ Termocoppia K – sul dispositivo        │
 * │ 0xD0   │ DEV_MARINER_BME280    │ Esterno      │ ESP01        │ BME280 – weather station               │
 * │ 0xE0   │ DEV_CALDAIA_DS18B20   │ Esterno      │ ESP8266      │ DS18B20 – caldaia                      │
 * ├────────┼───────────────────────┼──────────────┼──────────────┼────────────────────────────────────────┤
 * │ 0xFF   │ DEV_UNKNOWN           │ —            │ —            │ —                                      │
 * └────────┴───────────────────────┴──────────────┴──────────────┴────────────────────────────────────────┘
 */

// ── Controllers / actuators (0x00-0x0F) ──────────────────────────────────────
#define CTRL_MAIN_CONTROL    0x01   // Salotto – ESP8266 – toglie tensione ai servizi
#define CTRL_AIR_COND        0x02   // Corridoio – ESP8266 (futuro) – condizionatore
#define CTRL_HEAT_SYSTEM     0x03   // Esterno – ESP8266 – riscaldamento
#define CTRL_HOT_WATER       0x04   // Esterno – ESP8266 – acqua calda sanitaria
#define CTRL_TUYA_TENDE      0x05   // Casa – Node-RED bridge – tende Tuya

// ── Sensors (0x10-0xEF) ───────────────────────────────────────────────────────
#define DEV_CHRONO_DHT_1       0x10  // Salotto – CHRONO – DHT22 sul dispositivo
#define DEV_RPI_DHT_1          0x20  // Camera grande – Raspberry 3 – DHT22 sul dispositivo
#define DEV_BAGNO_DS18B20      0x30  // Bagno – ESP8266 Nextion – DS18B20
#define DEV_ENERGYMAIN_PZEM    0x40  // Salotto – ESP8266 – PZEM quadro elettrico
#define DEV_CAMINETTO_DS18B20  0x50  // Salotto – ESP8266 – DS18B20 caminetto (sensore 0)
#define DEV_CAMINETTO_SONDA_K  0x51  // Salotto – ESP8266 – Termocoppia K caminetto (sensore 1)
#define DEV_ESP_CAMERA         0x70  // Camera piccola – ESP8266 – Termocoppia K
#define DEV_MARINER_BME280     0xD0  // Esterno – ESP01 – BME280 weather station
#define DEV_CALDAIA_DS18B20    0xE0  // Esterno – ESP8266 – DS18B20 caldaia

// ── Reserved ─────────────────────────────────────────────────────────────────
// 0x06-0x0F  future controllers/actuators
// 0xF0-0xFE  test / development nodes
#define DEV_UNKNOWN            0xFF

#endif // DEVICES_H
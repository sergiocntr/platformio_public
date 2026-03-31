# Packet Protocol — Reference Card

> Source of truth: `PacketProtocol.h` · `PacketProtocol.cpp` · `devices.h`  
> Ultimo aggiornamento: 2026-03-28

---

## Frame layout

```
┌─────────┬─────────┬─────────┬──────────────┬──────────────────────────┬─────────┐
│ byte 0  │ byte 1  │ byte 2  │  bytes 3-4   │      bytes 5 … N         │ byte N+1│
├─────────┼─────────┼─────────┼──────────────┼──────────────────────────┼─────────┤
│  Magic  │ Version │  Type   │    Length    │         Payload          │   XOR   │
│  0xAA   │  0x01   │PacketType│ uint16 LE   │  deviceID + data bytes   │ 1 byte  │
└─────────┴─────────┴─────────┴──────────────┴──────────────────────────┴─────────┘
                        │                              │
                        │ HOW to read the payload      │ WHO sent the frame
                        ▼                              ▼
                    TYPE_*                          DEV_* (devices.h)
```

- **Header**: 5 byte fissi (`HEADER_SIZE = 5`)
- **Payload**: `payloadLength` byte (byte 3-4, little-endian); primo byte sempre `deviceID`
- **XOR**: calcolato su tutti i byte da 0 a N (header + payload, escluso se stesso)
- **Frame totale**: `5 + payloadLength + 1` byte
- **Frame minimo legale**: 6 byte (`PACKET_MIN_SIZE = 6`)

---

## Encoding conventions

Tutti i campi sensore sono `uint16_t` senza segno:

| Grandezza   | Encode (C sender)            | Decode (C receiver)          | Decode (Node.js / JS)                   |
|-------------|------------------------------|------------------------------|-----------------------------------------|
| temperature | `(float + 50.0) × 128`       | `val / 128.0f - 50.0f`       | `buf.readUInt16LE(off) / 128 - 50`      |
| humidity    | `float × 128`                | `val / 128.0f`               | `buf.readUInt16LE(off) / 128`           |
| pressure    | `float × 16`                 | `val / 16.0f`                | `buf.readUInt16LE(off) / 16`            |
| battery     | `mV` (raw, 1:1)              | `val` (mV)                   | `buf.readUInt16LE(off)`                 |
| blind pos   | `0-100` (raw, 1:1)           | `val` (%)                    | `buf.readUInt8(off)`                    |

**Range utili:**
- temperature: -50 … +462 °C (sufficiente per termocoppia K)
- humidity: 0 … 100 %
- pressure: 0 … 4095 hPa

**Macro C disponibili** (`PacketProtocol.h`):
```c
PP_ENCODE_TEMP(t)   PP_DECODE_TEMP(v)
PP_ENCODE_HUM(h)    PP_DECODE_HUM(v)
PP_ENCODE_PRESS(p)  PP_DECODE_PRESS(v)
```

---

## Packet types

| Valore | Nome           | Struct        | Uso                                                                         |
|--------|----------------|---------------|-----------------------------------------------------------------------------|
| `0x01` | TYPE_ANNOUNCE  | —             | E' un broadcast ESPNow: serve al gateway per riconoscere ilnodo             |
| `0x02` | TYPE_COMMAND   | —             | non sono dati ,questo e' un comando dal gateway al device                   |
| `0x03` | TYPE_METEO     | `meteoData`   | Stazione meteo outdoor (EEPROM-aligned)                                     |
| `0x04` | TYPE_DHT       | `dhtData`     | DHT22 e DS18B20 indoor                                                      |
| `0x05` | TYPE_BME       | `bmeData`     | BME280 senza batteria                                                       |
| `0x06` | TYPE_PZEM      | `EneMainData` | Energy meter PZEM                                                           |
| `0x07` | TYPE_TENDE     | `tendeData`   | Posizioni tende Tuya                                                        |
| `0xFF` | TYPE_UNKNOWN   | —             | Errore / sconosciuto                                                        |

---

## Strutture payload — dettaglio

### `TYPE_METEO = 0x03` → `struct meteoData` — stazione meteo outdoor

Dimensione fissa **16 byte** (EEPROM-aligned: 4 record per pagina da 64 byte).

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ byte 0   │ byte 1-2 │ byte 3-4 │ byte 5-6 │ byte 7-8 │ byte 9-10│byte 11-13│ byte 14  │ byte 15  │
│ deviceID │  humBMP  │  tempBMP │ pressure │ battery  │ moisture │ padding  │ counter  │ checksum │
│ uint8    │ int16 LE │ int16 LE │ int16 LE │ uint16 LE│ uint16 LE│ 3×uint8  │  uint8   │  uint8   │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
 Payload = 16 byte    Frame totale = 22 byte
```

> **Nota:** `humidityBMP`, `temperatureBMP`, `externalPressure` sono `int16_t` (con segno).
> `moisture` e `battery` sono opzionali: 0 se il sensore non è presente.

```javascript
// Node.js decode TYPE_METEO (offset dal frame completo, byte 5 = inizio payload)
const deviceID = buf.readUInt8(5);
const hum      = buf.readInt16LE(6)  / 128;
const temp     = buf.readInt16LE(8)  / 128 - 50;
const press    = buf.readInt16LE(10) / 16;
const battery  = buf.readUInt16LE(12);          // mV, 0 se assente
const moisture = buf.readUInt16LE(14);          // ADC raw, 0 se assente
const counter  = buf.readUInt8(19);
```

---

### `TYPE_DHT = 0x04` → `struct dhtData` — DHT22 e DS18B20 indoor

```
┌──────────┬──────────┬──────────┬──────────┐
│ byte 0   │ byte 1-2 │ byte 3-4 │  byte 5  │
│ deviceID │ humidity │   temp   │ comfort  │
│ uint8    │ uint16 LE│ uint16 LE│  uint8   │
└──────────┴──────────┴──────────┴──────────┘
 Payload = 6 byte    Frame totale = 12 byte
```

| Campo      | Tipo       | Note                                              |
|------------|------------|---------------------------------------------------|
| deviceID   | uint8      | vedi `devices.h`                                  |
| humidity   | uint16 LE  | `val / 128.0` → % ; sempre 0 per DS18B20          |
| temperature| uint16 LE  | `val / 128.0 - 50` → °C                           |
| comfort    | uint8      | bitmask: bit0=OK 1=TooHot 2=TooCold 3=TooDry 4=TooHumid |

```javascript
// Node.js decode TYPE_DHT
const deviceID = buf.readUInt8(5);
const hum      = buf.readUInt16LE(6) / 128;    // 0 per DS18B20
const temp     = buf.readUInt16LE(8) / 128 - 50;
const comfort  = buf.readUInt8(10);
```

---

### `TYPE_BME = 0x05` → `struct bmeData` — BME280 ambiente (senza batteria)

```
┌──────────┬──────────┬──────────┬──────────┐
│ byte 0   │ byte 1-2 │ byte 3-4 │ byte 5-6 │
│ deviceID │ humidity │   temp   │ pressure │
│ uint8    │ uint16 LE│ uint16 LE│ uint16 LE│
└──────────┴──────────┴──────────┴──────────┘
 Payload = 7 byte    Frame totale = 13 byte
```

> Per BME280 con batteria e moisture → usare `TYPE_METEO`.

```javascript
// Node.js decode TYPE_BME
const deviceID = buf.readUInt8(5);
const hum      = buf.readUInt16LE(6) / 128;
const temp     = buf.readUInt16LE(8) / 128 - 50;
const press    = buf.readUInt16LE(10) / 16;
```

---

### `TYPE_PZEM = 0x06` → `struct EneMainData` — energy meter

```
┌──────────┬──────────┬──────────┬──────────┬──────────┐
│ byte 0   │ byte 1-2 │ byte 3-4 │ byte 5-6 │ byte 7-8 │
│ deviceID │    V     │    I     │   cosφ   │    W     │
│ uint8    │ uint16 LE│ uint16 LE│ uint16 LE│ uint16 LE│
└──────────┴──────────┴──────────┴──────────┴──────────┘
 Payload = 9 byte    Frame totale = 15 byte
```

| Campo    | Decode              |
|----------|---------------------|
| v        | `val / 16.0` → V    |
| i        | `val / 128.0` → A   |
| c        | `val / 128.0` → cosφ|
| e        | `val` → W (raw)     |

```javascript
// Node.js decode TYPE_PZEM
const deviceID = buf.readUInt8(5);
const volt     = buf.readUInt16LE(6)  / 16;
const amp      = buf.readUInt16LE(8)  / 128;
const cosphi   = buf.readUInt16LE(10) / 128;
const watt     = buf.readUInt16LE(12);
```

---

### `TYPE_TENDE = 0x07` → `struct tendeData` — posizioni tende Tuya

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ byte 0   │  byte 1  │  byte 2  │  byte 3  │  byte 4  │  byte 5  │
│ deviceID │  pos[0]  │  pos[1]  │  pos[2]  │  pos[3]  │  pos[4]  │
│ uint8    │  uint8   │  uint8   │  uint8   │  uint8   │  uint8   │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
 Payload = 6 byte    Frame totale = 12 byte
```

- `pos[i]` = apertura tenda i-esima, 0–100 %
- `numTende = header.payloadLength - 1`

```javascript
// Node.js decode TYPE_TENDE
const deviceID = buf.readUInt8(5);
const n        = buf.readUInt16LE(3) - 1;   // payloadLength - 1
const pos      = [];
for (let i = 0; i < n; i++) pos[i] = buf.readUInt8(6 + i);
```

---

## DEV_* — registry dispositivi (WHO)

| deviceID | Costante               | Sensore       | PacketType  | Luogo          |
|----------|------------------------|---------------|-------------|----------------|
| `0x01`   | CTRL_MAIN_CONTROL      | attuatore     | TYPE_COMMAND| Salotto        |
| `0x02`   | CTRL_AIR_COND          | attuatore     | TYPE_COMMAND| Corridoio      |
| `0x03`   | CTRL_HEAT_SYSTEM       | attuatore     | TYPE_COMMAND| Esterno        |
| `0x04`   | CTRL_HOT_WATER         | attuatore     | TYPE_COMMAND| Esterno        |
| `0x05`   | CTRL_TUYA_TENDE        | bridge        | TYPE_TENDE  | Casa           |
| `0x10`   | DEV_CHRONO_DHT_1       | DHT22         | TYPE_DHT    | Salotto        |
| `0x20`   | DEV_RPI_DHT_1          | DHT22         | TYPE_DHT    | Camera grande  |
| `0x30`   | DEV_BAGNO_DS18B20      | DS18B20       | TYPE_DHT    | Bagno          |
| `0x40`   | DEV_ENERGYMAIN_PZEM    | PZEM          | TYPE_PZEM   | Salotto        |
| `0x50`   | DEV_CAMINETTO_DS18B20  | DS18B20       | TYPE_DHT    | Salotto        |
| `0x51`   | DEV_CAMINETTO_SONDA_K  | Termocoppia K | tbd         | Salotto        |
| `0x70`   | DEV_ESP_CAMERA         | Termocoppia K | tbd         | Camera piccola |
| `0xD0`   | DEV_MARINER_BME280     | BME280        | TYPE_METEO  | Esterno        |
| `0xE0`   | DEV_CALDAIA_DS18B20    | DS18B20       | TYPE_DHT    | Esterno        |
| `0xFF`   | DEV_UNKNOWN            | —             | —           | —              |

> **Aggiungere un nodo** → nuovo `DEV_*` in `devices.h`, nessun'altra modifica.  
> **Aggiungere un tipo sensore** → nuovo `TYPE_*` + struct in `PacketProtocol.h`.  
> Le due dimensioni sono indipendenti.

---

## Pattern C++ consigliato — ricezione

```cpp
// Early-return: nessun if annidato
if (buf[0] != PACKET_MAGIC || len < PACKET_MIN_SIZE) return;

ParsedPacket pkt;
if (pp_parsePacket(buf, len, &pkt) != 0) return;

switch (pkt.header.type) {

    case TYPE_METEO: {
        const meteoData *d = (const meteoData *)pkt.payload;
        float t  = d->temperatureBMP / 128.0f - 50.0f;
        float h  = d->humidityBMP    / 128.0f;
        float p  = d->externalPressure / 16.0f;
        // d->battery, d->moisture: 0 se assenti
        break;
    }
    case TYPE_DHT: {
        const dhtData *d = (const dhtData *)pkt.payload;
        float t = PP_DECODE_TEMP(d->temperature);
        float h = PP_DECODE_HUM(d->humidity);   // 0 per DS18B20
        // d->deviceID distingue DHT22 / DS18B20 / nodo
        break;
    }
    case TYPE_BME: {
        const bmeData *d = (const bmeData *)pkt.payload;
        float t = PP_DECODE_TEMP(d->temperature);
        float h = PP_DECODE_HUM(d->humidity);
        float p = PP_DECODE_PRESS(d->pressure);
        break;
    }
    case TYPE_PZEM: {
        const EneMainData *d = (const EneMainData *)pkt.payload;
        float v = d->v / 16.0f;
        float i = d->i / 128.0f;
        float c = d->c / 128.0f;
        uint16_t w = d->e;
        break;
    }
    case TYPE_TENDE: {
        const tendeData *d = (const tendeData *)pkt.payload;
        uint8_t n = pkt.header.payloadLength - 1;
        for (int i = 0; i < n && i < 5; i++) {
            uint8_t pos = d->pos[i];
        }
        break;
    }
}
```

---

## Note implementative

- **`meteoData` usa `int16_t`** (con segno) per i campi sensore — usare `readInt16LE` in JS, non `readUInt16LE`.
- **`dhtData` e `bmeData` usano `uint16_t`** (senza segno) — usare `readUInt16LE`.
- **DS18B20** usa `TYPE_DHT` con `humidity = 0` — il parser non necessita di logica speciale, è sufficiente ignorare il campo.
- **`TYPE_METEO`** è l'unico tipo con dimensione fissa non derivata dalla struct C pura (16 byte con padding esplicito) — validare sempre `payloadLength == 16`.
- **Termocoppia K** (`0x51`, `0x70`): `PacketType` ancora da definire — lasciare `TYPE_UNKNOWN` finché non viene implementato il firmware.
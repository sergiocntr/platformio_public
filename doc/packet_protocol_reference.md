# Packet Protocol — Reference Card

> Source of truth: `PacketProtocol.h` · `PacketProtocol.cpp` · `devices.h`  
> Ultimo aggiornamento: 2026-04-19 (v3.0 Resilient Star)

---

## Frame layout

```
┌─────────┬─────────┬─────────┬──────────────┬──────────────────────────┬─────────┐
│ byte 0  │ byte 1  │ byte 2  │  bytes 3-4   │      bytes 5 … N         │ byte N+1│
├─────────┼─────────┼─────────┼──────────────┼──────────────────────────┼─────────┤
│  Magic  │ Version │  Type   │    Length    │         Payload          │   XOR   │
│  0xAA   │  0x03   │PacketType│ uint16 LE   │  deviceID + data bytes   │ 1 byte  │
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

| Valore | Nome           | Struct         | Uso                                                                         |
|--------|----------------|----------------|-----------------------------------------------------------------------------|
| `0x00` | TYPE_ACK       | `ackData`      | ACK binario universale (deviceID + status + echoCmd + echoVal)              |
| `0x01` | TYPE_ANNOUNCE  | `announceData` | Handshake: riporta versione proto e firmware                                |
| `0x02` | TYPE_COMMAND   | `cmdData`      | Comandi dal gateway al device (relay, ventilazione, ecc.)                   |
| `0x03` | TYPE_METEO     | `meteoData`    | Stazione meteo outdoor (EEPROM-aligned, 16 byte)                            |
| `0x04` | TYPE_DHT       | `dhtData`      | DHT22 indoor (temp + umidità)                                               |
| `0x04` | TYPE_DS18      | `ds18Data`     | **Alias** di TYPE_DHT — DS18B20 temp-only (humidity = 0)                    |
| `0x05` | TYPE_BME       | `bmeData`      | BME280 ambiente (temp + hum + press + battery)                              |
| `0x06` | TYPE_PZEM      | `EneMainData`  | Energy meter PZEM                                                           |
| `0x07` | TYPE_TENDE     | `tendeData`    | Posizioni tende Tuya                                                        |
| `0x08` | TYPE_TIME      | `timeData`     | Sincronizzazione ora/giorno (4 byte payload)                                |
| `0x09` | TYPE_BOILER    | `boilerData`   | Telemetria caldaia (temp acqua + apertura valvola)                          |
| `0x0A` | TYPE_CAMINETTO | `caminettoData`| Nodo Caminetto (temp Ambiente + temp K + Fan PID)                           |
| `0x0B` | TYPE_PID_CONFIG| `pidConfigData`| Parametri PID ventola (mask + setPoint + P + I + D)                         |
| `0x0C` | TYPE_TENDE_COMMAND| `tendeCmdData`| Richiesta movimento tende (Chrono -> Bridge)                               |
| `0x0D` | TYPE_CLOUD_REPLY| —              | Risposta dal cloud (validazione dati Node-RED -> Cloud)                    |
| `0xFF` | TYPE_UNKNOWN   | —              | Errore / sconosciuto                                                        |

---

## Strutture payload — dettaglio

### `TYPE_ACK = 0x00` → `struct ackData` — conferma universale

Utilizzato per confermare la ricezione di un pacchetto dalGateway o dal Server.

```
┌──────────┬──────────┬──────────┬──────────┐
│ byte 0   │  byte 1  │  byte 2  │  byte 3  │
│ deviceID │  status  │ cmdEcho  │ valEcho  │
│ uint8    │  uint8   │  uint8   │  uint8   │
└──────────┴──────────┴──────────┴──────────┘
 Payload = 4 byte    Frame totale = 10 byte
```

- `deviceID`: Il destinatario/attuatore originale (es: `0x04` per acqua calda)
- `status`: 1=OK, 2=END, 3=FAILED, 4=ERROR, 5=SWITCH_TRANSPORT
- `cmdEcho`: Il comando originale che ha scatenato la risposta (es: `CMD_POWER_ON` o `PKT_ANNOUNCE`)
- `valEcho`: Lo stato reale risultante (es: `1` per ON, `0` per OFF)

**Nota (v3.3):** Lo status `0x05 (SWITCH_TRANSPORT)` viene usato dal Gateway in risposta a un `ANNOUNCE` via radio per comandare al nodo di spegnere il WiFi e restare solo in ESP-NOW.

**Nota (v2.1):** L'aggiunta di `cmdEcho` e `valEcho` permette al master (Chrono) di sincronizzare l'interfaccia UI solo dopo la conferma reale dall'attuatore, eliminando i loop di feedback.

---

### `TYPE_ANNOUNCE = 0x01` → `struct announceData` — handshake/discovery

Inviato all'avvio o alla connessione MQTT. Serve a mappare ID dispositivo e versioni.

```
┌──────────┬──────────┬──────────┐
│ byte 0   │  byte 1  │ byte 2-3 │
│ deviceID │ protoVer │  fwVer   │
│ uint8    │  uint8   │ uint16 LE│
└──────────┴──────────┴──────────┘
 Payload = 4 byte    Frame totale = 10 byte
```

- `deviceID`: id del mittente (es. `0x50`)
- `protoVer`: versione protocollo (attualmente `0x03`)
- `fwVer`: versione firmware (es. `26`)

---

### `TYPE_COMMAND = 0x02` → `struct cmdData` — comandi attuatori

```
┌──────────┬──────────┬──────────┐
│ byte 0   │  byte 1  │  byte 2  │
│ deviceID │ command  │  value   │
│ uint8    │  uint8   │  uint8   │
└──────────┴──────────┴──────────┘
 Payload = 3 byte    Frame totale = 9 byte
```

- `deviceID`: l'attuatore target (es. `0x03` per riscaldamento)
- `command`: id comando (CommandID)
- `value`: valore parametro (es. `1` per ON, `0` per OFF)

**Command IDs standard:**
- `0x00`: CMD_POWER_OFF
- `0x01`: CMD_POWER_ON
- `0xEE`: CMD_SYS_RESET
- `0xEF`: CMD_SYS_SLEEP — manda il device in deep-sleep; `value` = secondi (0 → default firmware). **Broadcast**: riconosciuto da qualsiasi device indipendentemente da `deviceID`; Node-RED lo usa come MQTT Last Will per spegnere la rete al fermo del broker.
- `0xF0`: CMD_SYS_UPDATE (Avvia `checkForUpdates()`)
- `0x90`: CMD_GET_NODES — Richiede al Gateway il report JSON dei nodi tracciati (pubblicato su `homie/log`). Il Gateway ID è `0xFE`.

---

## Gateway Node Monitoring (v3.2+)
Il Gateway ESP32 implementa ora un monitoraggio attivo della rete:
- **Tracciamento**: Ogni `deviceID` viene registrato con il proprio trasporto (ESP-NOW vs MQTT).
- **Firmware**: Estrapolazione della `fwVersion` dai pacchetti `TYPE_ANNOUNCE`.
- **Timeout (1h)**: I nodi non visti per più di 60 minuti vengono rimossi automaticamente dalla tabella (Cleanup).
- **Report JSON**: Risposta al comando `0x90` con elenco nodi, MAC, trasporto e tempo dall'ultimo avvistamento.

---

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

### `TYPE_BME = 0x05` → `struct bmeData` — BME280 ambiente

```
┌──────────┬──────────┬──────────┬──────────┬──────────┐
│ byte 0   │ byte 1-2 │ byte 3-4 │ byte 5-6 │ byte 7-8 │
│ deviceID │ humidity │   temp   │ pressure │ battery  │
│ uint8    │ uint16 LE│ uint16 LE│ uint16 LE│ uint16 LE│
└──────────┴──────────┴──────────┴──────────┴──────────┘
 Payload = 9 byte    Frame totale = 15 byte
```

> Per BME280 con moisture → usare `TYPE_METEO`. Il campo `battery` è presente anche in `bmeData` (mV raw).

```javascript
// Node.js decode TYPE_BME
const deviceID = buf.readUInt8(5);
const hum      = buf.readUInt16LE(6) / 128;
const temp     = buf.readUInt16LE(8) / 128 - 50;
const press    = buf.readUInt16LE(10) / 16;
const battery  = buf.readUInt16LE(12);          // mV, 0 se assente
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

### `TYPE_TENDE_COMMAND = 0x0C` → `struct tendeCmdData` — richiesta movimento

Utilizzato dal Chrono (o altro controller) per comandare il bridge Tuya.

```
┌──────────┬──────────┬──────────┬──────────┐
│ byte 0   │  byte 1  │  byte 2  │  byte 3  │
│ deviceID │ blindIdx │ command  │   val    │
│ uint8    │  uint8   │  uint8   │  uint8   │
└──────────┴──────────┴──────────┴──────────┘
 Payload = 4 byte    Frame totale = 10 byte
```

- `blindIdx`: indice della tenda (0–4)
- `command`: 15=STOP, 16=OPEN, 17=CLOSE, 3=POS
- `val`: valore percentuale (usato solo se command=3)

```javascript
// Node.js decode TYPE_TENDE_COMMAND
const deviceID = buf.readUInt8(5);
const t        = buf.readUInt8(6);   // blindIndex
const c        = buf.readUInt8(7);   // command
const p        = buf.readUInt8(8);   // percentage
```

---

### `TYPE_TIME = 0x08` → `struct timeData` — sincronizzazione ora/giorno

Inviato dal master (Node-RED / Raspberry Pi) a tutti i nodi per mantenere l'ora sincronizzata.

```
┌──────────┬──────────┬──────────┬──────────┐
│ byte 0   │  byte 1  │  byte 2  │  byte 3  │
│ deviceID │   hour   │  minute  │   day    │
│ uint8    │  uint8   │  uint8   │  uint8   │
└──────────┴──────────┴──────────┴──────────┘
 Payload = 4 byte    Frame totale = 10 byte
```

| Campo    | Tipo   | Note                                                                        |
|----------|--------|-----------------------------------------------------------------------------|
| deviceID | uint8  | Chi invia l'ora (di solito `CTRL_MAIN_CONTROL = 0x01`)                      |
| hour     | uint8  | 0–23                                                                        |
| minute   | uint8  | 0–59                                                                        |
| day      | uint8  | **0=Dom, 1=Lun, 2=Mar, 3=Mer, 4=Gio, 5=Ven, 6=Sab** (convenzione POSIX/JS)|

> **Nota:** il campo `day` usa la convenzione POSIX `tm_wday` (0=Domenica … 6=Sabato), uguale a quella di JavaScript `Date.getDay()`. Usare `WEEKDAY_SHORT[day]` / `WEEKDAY_LONG[day]` da `shared_config.h` con guard `day < 7`.

```javascript
// Node.js decode TYPE_TIME
const deviceID = buf.readUInt8(5);
const hour     = buf.readUInt8(6);
const minute   = buf.readUInt8(7);
const day      = buf.readUInt8(8);   // 0=Sun … 6=Sat  (POSIX)
```

---

### `TYPE_BOILER = 0x09` → `struct boilerData` — telemetria caldaia

```
┌──────────┬──────────┬──────────┐
│ byte 0   │ byte 1-2 │ byte 3-4 │
│ deviceID │   temp   │ valvePos │
│ uint8    │ uint16 LE│ uint16 LE│
└──────────┴──────────┴──────────┘
 Payload = 5 byte    Frame totale = 11 byte
```

| Campo    | Tipo      | Note                                              |
|----------|-----------|---------------------------------------------------|
| deviceID | uint8     | `DEV_CALDAIA_DS18B20 = 0xE0`                      |
| temp     | uint16 LE | `PP_DECODE_TEMP(val)` → °C (sonda DS18B20 caldaia)|
| valvePos | uint16 LE | `analogRead` raw (0–1023); 0 = chiusa             |

> **Nota:** `valvePos` non ha scaling — è il valore ADC grezzo. Se necessario mappare a % usare `val * 100 / 1023`.

```javascript
// Node.js decode TYPE_BOILER
const deviceID  = buf.readUInt8(5);
const temp      = buf.readUInt16LE(6) / 128 - 50;
const valvePos  = buf.readUInt16LE(8);             // ADC raw 0-1023
```

---

### `TYPE_CAMINETTO = 0x0A` → `struct caminettoData` — controllo camino

Specifico per il controllo ventilazione caminetto. Combina due sensori e l'output PWM.

```
┌──────────┬──────────┬──────────┬──────────┐
│ byte 0   │ byte 1-2 │ byte 3-4 │  byte 5  │
│ deviceID │ tempAmb  │  tempK   │  fanPid  │
│ uint8    │ uint16 LE│ uint16 LE│  uint8   │
└──────────┴──────────┴──────────┴──────────┘
 Payload = 6 byte    Frame totale = 12 byte
```

| Campo      | Tipo       | Note                                              |
|------------|------------|---------------------------------------------------|
| deviceID   | uint8      | Solitamente `0x50` (`DEV_CAMINETTO`)              |
| tempAmb    | uint16 LE  | `val / 128.0 - 50` → °C (Ambiente/Circuito)       |
| tempK      | uint16 LE  | `val / 128.0 - 50` → °C (Sonda K Camino)          |
| fanPid     | uint8      | Valore PWM ventola (0–255)                        |

```javascript
// Node.js decode TYPE_CAMINETTO
const deviceID = buf.readUInt8(5);
const tempAmb  = buf.readUInt16LE(6) / 128 - 50;
const tempK    = buf.readUInt16LE(8) / 128 - 50;
const fanPid   = buf.readUInt8(10);
```

---

### `TYPE_PID_CONFIG = 0x0B` → `struct pidConfigData` — setup parametri ventola

Permette aggiornamenti parziali tramite bitmask.

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ byte 0   │  byte 1  │  byte 2  │ bytes 3-6│bytes 7-10│bytes 11-14│bytes 15-18│bytes 19-22│
│ deviceID │ updateMsk│  sTemp   │ sMinFan  │ sMaxFan  │    P     │    I     │    D     │
│ uint8    │  uint8   │  uint8   │ float LE │ float LE │ float LE │ float LE │ float LE │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
 Payload = 23 byte    Frame totale = 29 byte
```

- `updateMask`: bitmask (bit0=sTemp, bit1=MinFan, bit2=MaxFan, bit3=P, bit4=I, bit5=D). **Bit7 (`0x80`) = Flag Comando da Master** (Node-RED). Serve per scartare l'eco MQTT.
- `sTemp`: Temperatura target
- `P, I, D`: Guadagni flottanti (4 byte IEEE 754)

---

### `TYPE_CLOUD_REPLY = 0x0D` — validazione cloud

Utilizzato esclusivamente dal sistema Node-RED per confermare l'invio riuscito dei dati al servizio cloud (per ora non gestito dai nodi C++).

```
┌──────────┬──────────┬──────────┐
│ byte 0   │  byte 1  │  byte 2  │
│ deviceID │  status  │   info   │
│ uint8    │  uint8   │  uint8   │
└──────────┴──────────┴──────────┘
 Payload = 2-3 byte    Frame totale = 8-9 byte
```

- `deviceID`: id del dispositivo a cui si riferiscono i dati (es. `0xD0`)
- `status`: `0=OK`, `1=FAILED`, `2=PARTIAL`
- `info`: (opzionale) `errorCode` o `recordCount`

```javascript
// Node.js decode TYPE_CLOUD_REPLY
const deviceID = buf.readUInt8(5);
const status   = buf.readUInt8(6);
const info     = buf.readUInt16LE(3) > 2 ? buf.readUInt8(7) : null;
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
| `0x40`   | DEV_ENERGYMAIN_PZEM    | PZEM          | TYPE_PZEM      | Salotto        |
| `0x50`   | DEV_CAMINETTO          | Multi         | TYPE_CAMINETTO | Salotto        |
| `0x70`   | DEV_ESP_CAMERA         | DHT22         | TYPE_DHT       | Camera piccola |
| `0xD0`   | DEV_MARINER_BME280     | BME280 +batt  | TYPE_METEO     | Esterno        |
| `0xE0`   | DEV_CALDAIA_DS18B20    | DS18B20       | TYPE_DHT       | Esterno        |
| `0xFF`   | DEV_MASTER             | Node-RED      | Multi          | Casa (Server)  |

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
- **DS18B20** standard usa `TYPE_DHT` con `humidity = 0` — il parser non necessita di logica speciale, è sufficiente ignorare il campo.
- **`TYPE_METEO`** è l'unico tipo con dimensione fissa non derivata dalla struct C pura (16 byte con padding esplicito) — validare sempre `payloadLength == 16`.E' cosi per salvare il dato nella EEPROM che ha 64 byte di pagina. (ne stanno 4 giuste senza andare 'a capo')
- **Caminetto** (`0x50`): Implementa `TYPE_CAMINETTO` che accorpa DS18B20 locale, Termocoppia K e stato ventola in un unico pacchetto da 6 byte di payload.

---

## Modello di Comunicazione: Telemetria Passiva vs Transazioni

Il protocollo differenzia nettamente due filosofie di trasmissione per minimizzare il traffico radio e supportare il low power, rispecchiate fedelmente nelle funzioni della libreria `mqttWifi`.

### 1. Telemetria (Fire & Forget / QoS 0)
Usata dai sensori per l'invio ciclico del proprio stato (`TYPE_DHT`, `TYPE_BME`, `TYPE_PZEM`, `TYPE_BOILER`, `TYPE_CAMINETTO`, ecc.).

- **Dinamica**: Il nodo trasmette il proprio buffer binario sul medium condiviso (es. il topic `espNowBridgeBuffer`) e prosegue immediatamente senza attendere nessun ACK.
- **Retain MQTT**: Deve essere **sempre disabilitato (`false`)**. Essendo il topic binario condiviso da tutti i device, un `retain=true` salverebbe indefinitamente il pacchetto casuale dell'ultimo device che ha trasmesso, causando decodifiche errate e asincrone a chiunque si iscriva (es. Node-RED in reboot o UI in refresh).
- **Dead Node Detection (Avarie)**: Demandata unicamente al master/gateway (Node-RED). Si implementa tramite watchdog sul ricevitore (pattern "Last Seen" / Timeout), **non** abusando di ACK su sensori passivi.
- **API `mqttWifi`**: Utilizzare la funzione standard:
  ```cpp
  mqttWifi::publish(espNowBridgeBuffer, buffer, packetSize, false);
  ```

### 2. Comandi (Transazioni Garantite)
Usata per impartire ordini (`TYPE_COMMAND`, `TYPE_TENDE_COMMAND`, `TYPE_PID_CONFIG`) o scambiare set di dati complessi (`TYPE_METEO` multi-record).

- **Dinamica**: Il nodo Master invia il pacchetto e **rimane attivamente in RX** in attesa di risposta.
- **Risposta**: La transazione è dichiarata conclusa solo alla ricezione di un vero pacchetto `TYPE_ACK` proveniente fisicamente dall'attuatore remoto. I bridge/gateway (come Node-RED) agiscono da ripetitori passivi e non auto-generano finti ACK per le transazioni di comando. 
- **API `mqttWifi`**: Utilizzare la funzione con retry integrato:
  ```cpp
  AckState status = mqttWifi::publishWithAck(espNowBridgeBuffer, buffer, packetSize);
  ```
- **DEPRECATA**: La funzione `void sendBinaryCommand(uint8_t deviceID, bool on)` e' deprecata : Utilizzare la funzione `publishWithAck(espNowBridgeBuffer, buffer, packetSize, false);`


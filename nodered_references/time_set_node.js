// Node-RED function node - versione con deviceId = 0xFF (broadcast)
const now = new Date();
const hour = now.getHours();
const minute = now.getMinutes();
const day = now.getDay(); // 0=Dom...6=Sab

const PKT_MAGIC = global.get("PKT_MAGIC") ?? 170;
const PKT_VERSION = global.get("PKT_VERSION") ?? 3;
const PKT_TYPE = global.get("TYPE_TIME") ?? 8;
const DEV_MASTER = global.get("DEVICE_NODE_RED") ?? 255;

// Costruisci il pacchetto
const payloadLength = 4;
const header = Buffer.from([PKT_MAGIC, PKT_VERSION, PKT_TYPE, payloadLength & 0xFF, (payloadLength >> 8) & 0xFF]);
const payload = Buffer.from([DEV_MASTER, hour, minute, day]);

// Calcola XOR su header+payload
let xor = 0;
const fullPacket = Buffer.concat([header, payload]);
for (let i = 0; i < fullPacket.length; i++) {
    xor ^= fullPacket[i];
}

// Aggiungi XOR checksum
const finalPacket = Buffer.concat([fullPacket, Buffer.from([xor])]);

msg.payload = finalPacket;
msg.topic = "homie/espNowBridge/buffer";
msg.deviceId = DEV_MASTER;
msg.packetSize = finalPacket.length; // Dovrebbe essere 10

node.warn(`Inviato TIME packet: ${hour.toString().padStart(2, '0')}:${minute.toString().padStart(2, '0')} Day:${day} XOR:${xor.toString(16)}`);

return msg;
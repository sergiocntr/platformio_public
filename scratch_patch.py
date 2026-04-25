import re

h_file = "/media/progetti_ext/PROJECT/platformio_public/mqttWifi/mqttWifi.h"
with open(h_file, "r") as f:
    text = f.read()
if "extern uint8_t m_deviceID" not in text:
    text = text.replace("extern char m_mqtt_id[20];", "extern char m_mqtt_id[20];\nextern uint8_t m_deviceID;")
    with open(h_file, "w") as f:
        f.write(text)

cpp_file = "/media/progetti_ext/PROJECT/platformio_public/mqttWifi/mqttWifi_transport.cpp"
with open(cpp_file, "r") as f:
    text = f.read()

text = re.sub(
    r"buf\[5\] = 0xFE;\s*// Temp ID per manshake\s*buf\[6\] = 0x03;\s*// protoVer\s*buf\[7\] = 0x00;\s*// fwVer LSB\s*buf\[8\] = 0x00;\s*// fwVer MSB",
    "buf[5] = mqttWifi::m_deviceID;\n    buf[6] = 0x03; // protoVer\n    extern uint16_t versione;\n    buf[7] = (uint8_t)(versione & 0xFF);\n    buf[8] = (uint8_t)(versione >> 8);",
    text
)
with open(cpp_file, "w") as f:
    f.write(text)

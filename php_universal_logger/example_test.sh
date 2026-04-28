#!/bin/bash

# Example of calling the new PHP API with a hex buffer
# Magic: AA (170)
# Version: 04
# Type: 0A (CAMINETTO = 10)
# Payload Len: 0006 (6 bytes)
# Payload: 
#   DeviceId: 50 (80)
#   tempAmb: 7820 (Uint16LE) -> 8312 / 128 - 50 = 14.93
#   tempK: 4030 (Uint16LE) -> 12352 / 128 - 50 = 46.5
#   fanPid: FF (255)
#   XOR: (not checked in this simple PHP version but part of protocol)

ENDPOINT="https://www.developteamgold.altervista.org/php_universal_logger/api_endpoint.php"

# Mock hex payload
HEX_PAYLOAD="aa040a06005020783040ff"

echo "Sending CAMINETTO data (Hex)..."
printf "$(echo $HEX_PAYLOAD | sed 's/../\\x&/g')" | curl -X POST -H "Content-Type: application/octet-stream" --data-binary @- $ENDPOINT
echo -e "\n"

# JSON payload example
echo "Sending generic JSON data..."
curl -X POST -H "Content-Type: application/json" -d '{"type": "CAMINETTO", "data": {"deviceId": 80, "tempAmb": 22.5, "tempK": 65.0, "fanPid": 128}}' $ENDPOINT
echo -e "\n"

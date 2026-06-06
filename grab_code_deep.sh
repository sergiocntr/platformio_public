#!/bin/bash
# DeepSeek Optimized Scraper - Massima densità informativa
HARDCODED_PATHS=(
    "/media/progetti_ext/PROJECT/platformio_public"
    "/media/progetti_ext/PROJECT/Platformio/finiti/chrono2/src_crono"
    "/media/progetti_ext/PROJECT/Platformio/finiti/chrono2/src_crono_bagno"
    "/media/progetti_ext/PROJECT/Platformio/finiti/energy/energyMain/src"
    "/media/progetti_ext/PROJECT/Platformio/finiti/nodecaldaia/src"
    "/media/progetti_ext/PROJECT/Platformio/ESP8266/ESP_Caminetto/src"
    "/media/progetti_ext/PROJECT/Platformio/EspNowGateway/src"
)
process_dir() {
    TARGET_DIR="${1%/}"
    [ ! -d "$TARGET_DIR" ] && {
        echo "Skip: $TARGET_DIR"
        return
    }
    PARENT_FOLDER=$(basename "$(dirname "$TARGET_DIR")")
    OUTPUT_FILE="./txt/${PARENT_FOLDER}.txt"
    >"$OUTPUT_FILE"
    echo ">>> $TARGET_DIR -> $OUTPUT_FILE"
    find "$TARGET_DIR" -type f \( -name "*.ino" -o -name "*.cpp" -o -name "*.h" -o -name "*.c" \) -print0 | while IFS= read -r -d '' file; do
        echo "" >>"$OUTPUT_FILE"
        echo "//=== $file ===" >>"$OUTPUT_FILE"
        echo "" >>"$OUTPUT_FILE"
        cat "$file" | sed '/^[[:space:]]*$/d' >>"$OUTPUT_FILE" # Rimuove righe vuote DENTRO i file
    done
    echo "Done: $OUTPUT_FILE ($(wc -l <"$OUTPUT_FILE") lines)"
}
if [ -z "$1" ]; then
    for DIR in "${HARDCODED_PATHS[@]}"; do process_dir "$DIR"; done
else
    process_dir "$1"
fi

#!/bin/bash

# Lista hardcoded di percorsi da processare quando non viene fornito un argomento
HARDCODED_PATHS=(
    "/media/progetti_ext/PROJECT/platformio_public"
    "/media/progetti_ext/PROJECT/Platformio/finiti/chrono2/src_crono"
    "/media/progetti_ext/PROJECT/Platformio/finiti/chrono2/src_crono_bagno"
    "/media/progetti_ext/PROJECT/Platformio/finiti/energy/energyMain/src"
    "/media/progetti_ext/PROJECT/Platformio/finiti/nodecaldaia/src"
    "/media/progetti_ext/PROJECT/Platformio/ESP8266/ESP_Caminetto/src"
    "/media/progetti_ext/PROJECT/Platformio/EspNowGateway/src"
)

# 1. Controllo se l'argomento è stato fornito
if [ -z "$1" ]; then
    echo "Nessun argomento fornito. Utilizzo la lista hardcoded..."
    
    # Processa ogni percorso nella lista hardcoded
    for TARGET_DIR in "${HARDCODED_PATHS[@]}"; do
        # Rimuovo eventuali slash finali dal nome della cartella per pulizia
        TARGET_DIR="${TARGET_DIR%/}"
        
        # Controllo se la directory esiste
        if [ ! -d "$TARGET_DIR" ]; then
            echo "Warning: La cartella '$TARGET_DIR' non esiste. Salto..."
            continue
        fi
        
        # Ottengo il nome della cartella padre (quella che contiene 'src')
        PARENT_FOLDER=$(basename "$(dirname "$TARGET_DIR")")
        OUTPUT_FILE="./txt/${PARENT_FOLDER}.txt"
        
        # Svuota il file di output se esiste già
        >"$OUTPUT_FILE"
        
        echo "Analisi in corso della cartella: $TARGET_DIR..."
        echo "Output destinato a: $OUTPUT_FILE"
        
        # Ricerca ricorsiva all'interno della cartella specificata
        find "$TARGET_DIR" -type f \( -name "*.ino" -o -name "*.cpp" -o -name "*.h" -o -name "*.c" -o -name "*.md" \) | while read -r file; do
            echo "Aggiungo: $file"
            echo -e "\n// ==========================================" >>"$OUTPUT_FILE"
            echo "// FILE: $file" >>"$OUTPUT_FILE"
            echo -e "// ==========================================\n" >>"$OUTPUT_FILE"
            
            cat "$file" >>"$OUTPUT_FILE"
            echo -e "\n" >>"$OUTPUT_FILE"
        done
        
        echo "--------------------------------------------------"
        echo "Completato! File generato: $OUTPUT_FILE"
        echo ""
    done
    
    exit 0
fi

# Se viene fornito un argomento, processa solo quella cartella
TARGET_DIR="${1%/}"

# Controllo se la directory esiste
if [ ! -d "$TARGET_DIR" ]; then
    echo "Errore: La cartella '$TARGET_DIR' non esiste."
    exit 1
fi

# Ottengo il nome della cartella padre (quella che contiene 'src')
PARENT_FOLDER=$(basename "$(dirname "$TARGET_DIR")")
OUTPUT_FILE="./txt/${PARENT_FOLDER}.txt"

# Svuota il file di output se esiste già
>"$OUTPUT_FILE"

echo "Analisi in corso della cartella: $TARGET_DIR..."
echo "Output destinato a: $OUTPUT_FILE"

# 4. Ricerca ricorsiva all'interno della cartella specificata
find "$TARGET_DIR" -type f \( -name "*.ino" -o -name "*.cpp" -o -name "*.h" -o -name "*.c" -o -name "*.md" \) | while read -r file; do
    echo "Aggiungo: $file"
    echo -e "\n// ==========================================" >>"$OUTPUT_FILE"
    echo "// FILE: $file" >>"$OUTPUT_FILE"
    echo -e "// ==========================================\n" >>"$OUTPUT_FILE"
    
    cat "$file" >>"$OUTPUT_FILE"
    echo -e "\n" >>"$OUTPUT_FILE"
done

echo "--------------------------------------------------"
echo "Completato! File generato: $OUTPUT_FILE"
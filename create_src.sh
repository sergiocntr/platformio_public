#!/bin/bash

#!/bin/bash

# Array con le cartelle delle librerie da processare
librerie=(
    "PacketProtocol" "SensorManager" "shared_config"
    "gsm_lib"
    "log_lib"
    "mqttWifi"
    "NexManager"
    "nodeRelay" "definitions" 
)

# Ottieni la directory corrente (root del progetto)
ROOT_DIR=$(pwd)

# Array per raccogliere i symlink
symlinks=()

# Ciclo per creare cartella src, spostare i file e creare library.json
for lib in "${librerie[@]}"; do
    if [ -d "$lib" ]; then
        echo "Processing $lib..."

        # Crea la cartella src se non esiste
        mkdir -p "$lib/src"

        # Sposta i file .cpp e .h in src (senza generare errori se non ci sono)
        mv "$lib"/*.cpp "$lib"/*.h "$lib/src/" 2>/dev/null

        # Crea library.json solo se non esiste già
        if [ ! -f "$lib/library.json" ]; then
            cat >"$lib/library.json" <<EOF
{
    "name": "$lib",
    "version": "1.0.0",
    "keywords": "",
    "description": "",
    "repository": {
        "type": "",
        "url": ""
    },
    "authors": [],
    "license": "",
    "frameworks": "*",
    "platforms": "*",
    "build": {
        "srcDir": "src"
    }
}
EOF
            echo "  ✓ Creato library.json in $lib"
        else
            echo "  ✓ library.json già esistente in $lib, saltato"
        fi

        # Crea il symlink assoluto
        # Accumula il symlink nell'array
        symlinks+=("symlink://$ROOT_DIR/$lib")

        echo "✓ $lib done"
    else
        echo "⚠️  $lib not found, skipping..."
    fi
done

# Stampa tutti i symlink alla fine
echo "================================================"
echo "📌 Copia questi symlink nel tuo platformio.ini:"
echo "================================================"
echo "lib_deps ="
for symlink in "${symlinks[@]}"; do
    echo "    $symlink"
done
echo "================================================"

echo ""
echo "✅ Tutto completato!"

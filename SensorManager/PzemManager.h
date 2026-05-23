#ifndef PZEM_MANAGER_H
#define PZEM_MANAGER_H

#include <PZEM004Tv30.h>
#include <shared_config.h>

class PzemManager {
public:
    // Accetta un riferimento all'oggetto PZEM già esistente nel progetto
    PzemManager(PZEM004Tv30& pzemDevice) : _pzem(pzemDevice) {}

    void setup() {
        // PZEM non ha un vero .begin() che ritorna bool, verifichiamo con una lettura
        update();
    }

    void update() {
        float v = _pzem.voltage();
        if (!isnan(v)) {
            // Se la tensione è leggibile, il sensore è vivo
            stato.sensor_alive[0] = true;
        } else {
            stato.sensor_alive[0] = false;
        }
    }

private:
    PZEM004Tv30& _pzem;
};

#endif

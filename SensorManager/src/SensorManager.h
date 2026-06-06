#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <shared_config.h>

/**
 * @file SensorManager.h
 * @brief Gestione centralizzata dei sensori e della loro "salute" (sensor_alive).
 */

namespace SensorManager {

    // Indici standard per la bitmask di salute (max 4 sensori come in SystemState)
    enum SensorIndex {
        MAIN_SENS = 0,
        SEC_SENS  = 1,
        THIRD_SENS = 2,
        FOURTH_SENS = 3
    };

    /**
     * Inizializza i sensori configurati per il nodo.
     * Deve essere chiamata nel setup().
     */
    void setup();

    /**
     * Esegue la lettura dei sensori e aggiorna SystemState.
     * Gestisce automaticamente il flag sensor_alive per ogni sensore.
     */
    void update();

    /**
     * Restituisce la bitmask dello stato di salute.
     * Ogni bit i-esimo corrisponde a stato.sensor_alive[i].
     */
    inline uint8_t getHealthMask() {
        uint8_t mask = 0;
        for (int i = 0; i < 4; i++) {
            if (stato.sensor_alive[i]) mask |= (1 << i);
        }
        return mask;
    }

} // namespace SensorManager

#endif // SENSOR_MANAGER_H

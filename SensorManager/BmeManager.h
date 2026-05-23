#ifndef BME_MANAGER_H
#define BME_MANAGER_H

#include <BME280I2C.h>
#include <shared_config.h>

/**
 * @class BmeManager
 * @brief Gestisce il sensore BME280 per la libreria SensorManager.
 */
class BmeManager {
public:
    BmeManager() : _bme(_settings) {}

    void setup() {
        if (!_bme.begin()) {
            stato.sensor_alive[0] = false;
        } else {
            stato.sensor_alive[0] = true;
        }
    }

    void update() {
        float temp = NAN, hum = NAN, pres = NAN;
        _bme.read(pres, temp, hum);
        
        if (isnan(temp) || isnan(hum) || isnan(pres)) {
            stato.sensor_alive[0] = false;
        } else {
            stato.temps[0] = temp;
            stato.hums[0] = hum;
            // Pressione non mappata in SystemState standard, 
            // ma il manager aggiorna la "salute".
            stato.sensor_alive[0] = true;
        }
    }

private:
    BME280I2C::Settings _settings = BME280I2C::Settings(
        BME280::OSR_X1, BME280::OSR_X1, BME280::OSR_X1,
        BME280::Mode_Forced, BME280::StandbyTime_1000ms,
        BME280::Filter_Off, BME280::SpiEnable_False
    );
    BME280I2C _bme;
};

#endif // BME_MANAGER_H

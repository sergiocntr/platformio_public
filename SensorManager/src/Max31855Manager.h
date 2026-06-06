#ifndef MAX31855_MANAGER_H
#define MAX31855_MANAGER_H

#include <MAX31855.h>
#include <shared_config.h>

class Max31855Manager {
public:
    Max31855Manager(uint8_t clk, uint8_t cs, uint8_t do_pin, uint8_t stateIndex = 1) 
        : _clk(clk), _cs(cs), _do(do_pin), _idx(stateIndex) {}

    void setup() {
        _sensor.begin(_clk, _cs, _do);
        update();
    }

    void update() {
        uint8_t status = _sensor.read();
        if (status == 0) {
            stato.temps[_idx] = _sensor.getTemperature();
            stato.sensor_alive[_idx] = true;
        } else {
            stato.sensor_alive[_idx] = false;
        }
    }

private:
    MAX31855 _sensor;
    uint8_t _clk, _cs, _do, _idx;
};

#endif

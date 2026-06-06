#ifndef DS18_MANAGER_H
#define DS18_MANAGER_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include <shared_config.h>

class Ds18Manager {
public:
    Ds18Manager(uint8_t pin, uint8_t stateIndex = 0) 
        : _oneWire(pin), _sensors(&_oneWire), _idx(stateIndex) {}

    void setup() {
        _sensors.begin();
         this->update();
       // stato.sensor_alive[_idx] = (_sensors.getDeviceCount() > 0);
    }

    void update() {
        _sensors.requestTemperatures();
        float t = _sensors.getTempCByIndex(0);
        if (t != 85.0 && t != -127.0) {
            stato.temps[_idx] = t;
            stato.sensor_alive[_idx] = true;
        } else {
            stato.sensor_alive[_idx] = false;
        }
    }

private:
    OneWire _oneWire;
    DallasTemperature _sensors;
    uint8_t _idx;
};

#endif

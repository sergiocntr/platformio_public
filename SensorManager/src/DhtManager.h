#ifndef DHT_MANAGER_H
#define DHT_MANAGER_H

#include <DHTesp.h>
#include <shared_config.h>

class DhtManager {
public:
  DhtManager(uint8_t pin) : _pin(pin) {}

  void setup() {
    _dht.setup(_pin, DHTesp::DHT22);
    _dht.getTempAndHumidity();
    // Verifichiamo se il sensore risponde
    stato.sensor_alive[0] = (_dht.getStatus() == DHTesp::ERROR_NONE);
  }

  void update() {
    ComfortState cf;
    TempAndHumidity values = _dht.getTempAndHumidity();

    if (_dht.getStatus() == DHTesp::ERROR_NONE) {
      stato.temps[0] = values.temperature;
      stato.hums[0] = values.humidity;
      _dht.getComfortRatio(cf, values.temperature, values.humidity);
      stato.ComfortRatio[0] = (1 << (uint8_t)cf);
      stato.sensor_alive[0] = true;
    } else {
      stato.sensor_alive[0] = false;
    }
  }

private:
  uint8_t _pin;
  DHTesp _dht;
};

#endif

#ifndef MOISTURE_MANAGER_H
#define MOISTURE_MANAGER_H

#include <Arduino.h>

class MoistureManager {
public:
    MoistureManager(uint8_t powerPin, uint8_t adcPin, uint16_t dryValue, uint16_t wetValue)
        : _pwrPin(powerPin), _adcPin(adcPin), _dryValue(dryValue), _wetValue(wetValue) {}

    void setup() {
        pinMode(_pwrPin, OUTPUT);
        digitalWrite(_pwrPin, LOW); // Spento inizialmente
        pinMode(_adcPin, INPUT);

        // Dummy reads per scaldare l'ADC
        for (int i = 0; i < 5; i++) {
            analogRead(_adcPin);
            delay(10);
        }
    }

    bool update(uint16_t &rawValue, uint16_t &percentValue) {
        digitalWrite(_pwrPin, HIGH);
        delay(200);

        uint32_t sum = 0;
        for (int i = 0; i < 10; i++) {
            sum += analogRead(_adcPin);
            delay(10);
        }
        digitalWrite(_pwrPin, LOW);

        rawValue = sum / 10;

        // Sentinella d'errore (disconnesso o in corto)
        if (rawValue == 0 || rawValue >= 4095) {
            percentValue = 0xFFFE;
            return false;
        }

        if (rawValue > _wetValue) {
            percentValue = rawValue;
        } else if (rawValue < _dryValue) {
            percentValue = rawValue;
        } else {
            percentValue = map(rawValue, _dryValue, _wetValue, 0, 100);
        }

        return true;
    }

private:
    uint8_t _pwrPin;
    uint8_t _adcPin;
    uint16_t _dryValue;
    uint16_t _wetValue;
};

#endif

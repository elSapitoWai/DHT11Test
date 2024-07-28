#ifndef DHT_H
#define DHT_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <wiringPi.h>

#define microsecondsToClockCycles(a) ((a) * (SystemCoreClock / 1000000L))

typedef struct {
				uint8_t pin;
				uint8_t type;
				uint8_t count;
} DHT;

// Use 55 as default value
void begin(DHT* dht, uint8_t microseconds);

float readTemperature(DHT* dht, bool magnitude, bool forceMode);
float convertCelsiusToFahrenheit(float value);
float convertFahrenheitToCelsius(float value);
float computeHeatIndex(float tempetature, float percentHumidity, bool isFahrenheit);
float readHumidity(DHT* dht, bool forceMode);
bool read(DHT* dht, bool forceMode);

uint32_t expectPulse(DHT* dht, bool level);

#endif

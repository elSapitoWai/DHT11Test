#include "DHT.h"
#include <math.h>
#include <stdlib.h>

#define MIN_INTERVAL 2000
#define TIMEOUT UINT32_MAX

static const uint8_t DHT11 = 11;       
static const uint8_t DHT12 = 12;       
static const uint8_t DHT21 = 21;       
static const uint8_t DHT22 = 22;       
static const uint8_t AM2301 = 21;

uint32_t lastReadTime, maxCycles;
uint8_t pullTime;
bool lastResult;

uint8_t data[5];

void begin(DHT* dht, uint8_t microseconds) {
				printf("Initializing DHT\n");
				pinMode(dht->pin, INPUT);

				lastReadTime = millis() - MIN_INTERVAL;
				pullTime = microseconds;
}

float readTemperature(DHT* dht, bool magnitude, bool forceMode) {
				float f;

				printf("Reading temp\n");

				if (read(dht, forceMode)) {
								switch (dht->type) {
												case 11:
																printf("Detected DHT11\n");
																f = data[2];
																if (data[3] & 0x80) {
																				f = -1 - f;
																}
																f += (data[3] & 0x0f) * 0.1;
																if (magnitude) {
																				f = convertCelsiusToFahrenheit(f);
																}
																printf("Temp is: %s\n", f);
																break;
												case 12:
																f = data[2];
																f += (data[3] & 0x0f) * 0.1;
																if (data[2] & 0x80) {
																				f *= -1;
																}
																if (magnitude) {
																				f = convertCelsiusToFahrenheit(f);
																}
																break;
												case 22:
												case 21:
																f = ((uint8_t)(data[2] & 0x7F)) << 8 | data[3];
																f *= 0.1;
																if (data[2] & 0x80) {
																				f *= -1;
																}
																if (magnitude) {
																				f = convertCelsiusToFahrenheit(f);
																}
																break;
								}
				} else {
								printf("Error while reading");
				}

				return f;
}

float convertCelsiusToFahrenheit(float value) { return value * 1.8 + 32; }

float convertFahrenheitToCelsius(float value) { return (value - 32) * 0.55555; }

float readHumidity(DHT* dht, bool forceMode) {
				float f;
				if (read(dht, forceMode)) {
								switch (dht->type) {
												case 11:
												case 12:
																f = data[0] + data[1] * 0.1;
																break;
												case 22:
												case 21:
																f = ((uint8_t)data[0]) << 8 | data[1];
																f *= 0.1;
																break;
								}
				}
				return f;
}

float computeHeatIndex(float temperature, float percentHumidity, bool isFahrenheit) {
				float heatIndex;

				if (!isFahrenheit)
								temperature = convertCelsiusToFahrenheit(temperature);

				heatIndex = 0.5 * (temperature + 61.0 + ((temperature - 68.0) * 1.2) + (percentHumidity * 0.094));

				if (heatIndex > 79) {
								heatIndex = -42.379 + 2.04901523 * temperature + 10.14333127 * percentHumidity + 
												-0.22475541 * temperature * percentHumidity +
												-0.00683783 * pow(temperature, 2) + 
												-0.05481717 * pow(percentHumidity, 2) + 
												0.00122874 * pow(temperature, 2) * percentHumidity + 
												0.00085282 * temperature * pow(percentHumidity, 2) + 
												-0.00000199 * pow(temperature, 2) * pow(percentHumidity, 2);

								if ((percentHumidity < 13) && (temperature >= 80.0) && (temperature <= 112.0))
												heatIndex -= ((13.0 - percentHumidity) * 0.25) * 
																sqrt((17.0 - abs(temperature - 95.0)) * 0.05882);
								else if ((percentHumidity > 85.0) && (temperature >= 80.0) && (temperature <= 87.0))
												heatIndex += ((percentHumidity - 85.0) * 0.1) * ((87.0 - temperature) * 0.2);
				}

				return isFahrenheit ? heatIndex : convertFahrenheitToCelsius(heatIndex);
}

bool read(DHT* dht, bool forceMode) {
				printf("Reading data\n");

				uint32_t currentTime = millis();
				if (!forceMode && ((currentTime - lastReadTime) < MIN_INTERVAL)) {
								return lastResult;
				}
				lastReadTime = currentTime;
				
				data[0] = data[1] = data[2] = data[3] = data[4] = 0;

				pinMode(dht->pin, INPUT);
				delay(1);
				
				// Set data line low for a period acoording to sensor type
				pinMode(dht->pin, OUTPUT);
				digitalWrite(dht->pin, LOW);
				switch (dht->type) {
								case 22:
								case 21:
												delayMicroseconds(1100); // data sheet says "at least 1ms"
												break;
								case 11:
								default:
												delay(20); // data sheet says at least 18ms, 20ms just to be safe
												break;
				}

				uint32_t cycles[80];
				{
								// End the start signal by setting data line high for 40 microseconds.
								pinMode(dht->pin, INPUT);
								
								// Delay a moment to let sensor pull data line low.
								delayMicroseconds(pullTime);

								// Now start reading the data line to get the value from the DHT sensor
								
								// TODO: This methods are used for printing debug data
								if (expectPulse(dht, LOW) == TIMEOUT) {
											  printf("Shit");	// DHT timeout waiting for start signal low pulse
												lastResult = false;
												return lastResult;
								}
								
								if (expectPulse(dht, HIGH) == TIMEOUT) {
												// DHT timeout waiting for start signal high pulse
												lastResult = false;
												return lastResult;
								}
								// Now read the 40 bits sent by the sensor. Each bit is sent as a 50 microsecond low pulse
								// followed by a variable length high pulse. If the high pulse is ~28 microseconds then
								// it's a 0 and if it's ~70 microsenconds then it's a 1. We measure the cycle count of the
								// initial 50 µs low pulse and use that to compare to the cycle count of the high pulse to
								// determine if the bit is a 0 (high state cycle count < low state cycle count). Note that
								// for speed all the pulses are read into an array and then examined in a later step.
								
								for (int i = 0; i < 40; ++i) {
												uint32_t lowCycles = cycles[2 * i];
												uint32_t highCycles = cycles[2 * i + 1];

												if ((lowCycles == TIMEOUT) || (highCycles == TIMEOUT)) {
																// TODO: another debug message
																// DHT timeout waiting for pulse
																lastResult = false;
																return lastResult;
												}
												data [i / 8] <<= 1;

												// Now compare the low and high cycle times to see if the bit is a 0 or a 1;
												if (highCycles > lowCycles) {
																data[i / 8] |= 1;
												}

												// Else high cycles are less than ( or equal to, a weird case) the 50 µs low
												// cycle count so this must be a zero. Nothing needs to be changed in the 
												// stored data.
								}

								// TODO: more debug data that prints the received data.
								
								// Check we read 40 bits and that the checksum matches.
								if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
												lastResult = true;
												return lastResult;
								} else {
												// TODO: Debug print
												// DHT checksum failure
												lastResult = false;
												return lastResult;
								}
				}

				printf("%s, %s, %s, %s, %s", data[0], data[1], data[2], data[3], data[4]);
}

uint32_t expectPulse(DHT* dht, bool level) {
				#if (F_CPU > 16000000L) || (F_CPU == 0L)
								uint32_t count = 0;
				#else
								uint16_t count = 0; // To work fast enough on slower boards
				#endif

				while (digitalRead(dht->pin) == level) {
								if (count++ >= maxCycles) {
												return TIMEOUT;
								}
				}

				return count;
}



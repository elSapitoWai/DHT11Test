#include <wiringPi.h>
#include <stdlib.h>
#include <stdio.h>
#include "DHT.h"

#define DHT_PIN 12

int main() {
	printf( "Raspberry Pi wiringPi DHT11 Temperature test program\n" );

	if ( wiringPiSetup() == -1 )
		exit( 1 );

  DHT dht;
	dht.pin = DHT_PIN;
  dht.type = 11;
	dht.count = 1;

  begin(&dht, 55);

  float temp = readTemperature(&dht, false, true);

  printf("%s", temp);

	return 0;
}

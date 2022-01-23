#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <wiringPi.h>

static int PIN_TRIGG = 22;
static int PIN_START = 23;
static int PIN_STOP = 24;

int main(int argc, char *argv[])
{
	wiringPiSetupGpio();

	pinMode(PIN_TRIGG, INPUT);
	pinMode(PIN_START, OUTPUT);
	pinMode(PIN_STOP, OUTPUT);

	digitalWrite(PIN_START, 0);
	digitalWrite(PIN_STOP, 0);

	int e = 0;

	while (1) {

		// wait for trig
		while (digitalRead(PIN_TRIGG) == 0);

		digitalWrite(PIN_START, 1);
		digitalWrite(PIN_STOP, 1);
		digitalWrite(PIN_START, 0);
		digitalWrite(PIN_STOP, 0);

	}

	return 0;
}

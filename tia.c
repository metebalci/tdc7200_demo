#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <wiringPi.h>

static double CLOCK = 5e6; // MHz

static int PIN_INTB = 25;
static int PIN_ENABLE = 27;

static int fd = 0;
static const char *device = "/dev/spidev0.0";
static uint32_t mode = SPI_MODE_0;
static uint8_t bits = 8;
static uint32_t speed = 2000000;

static void pabort(const char *s)
{
	perror(s);
	digitalWrite(PIN_ENABLE, 0);
	if (fd >= 0) close(fd);
	abort();
}

// convert rx[4] buffer to 23 bit response
static uint32_t get23(uint8_t* r)
{
	uint32_t t = (r[1] << 16) | (r[2] << 8) | (r[3]);
	return t & 0x7FFFFF;
}

static void transfer(int fd, uint8_t const *tx, uint8_t const *rx, size_t len)
{
	int ret;

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = len,
		.delay_usecs = 0,
		.speed_hz = speed,
		.bits_per_word = 8,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

}

int comp (const void * elem1, const void * elem2) 
{
    double f = *((double*)elem1);
    double s = *((double*)elem2);
    if (f > s) return  1;
    if (f < s) return -1;
    return 0;
}

int main(int argc, char *argv[])
{
	double CLOCKperiod = 1e9/CLOCK; // ns

	int ret = 0;

	fd = open(device, O_RDWR);
	if (fd < 0)
		pabort("can't open device");

	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE32, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE32, &mode);
	if (ret == -1)
		pabort("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");

	wiringPiSetupGpio();

	pinMode(PIN_INTB, INPUT);
	pullUpDnControl(PIN_INTB, PUD_UP);
	pinMode(PIN_ENABLE, OUTPUT);

	digitalWrite(PIN_ENABLE, 0);

	uint8_t tx[4];
	uint8_t rx[4];

	// enable TDC
	digitalWrite(PIN_ENABLE, 1);

	// set int mask
	// enable only measurement done
	tx[0] = 0x43;
	tx[1] = 0x01;
	transfer(fd, tx, rx, 2);

	int num_measurements = 10000;
	int num_valid_measurements = 0;

	double m[100000];

	while (num_valid_measurements < num_measurements) {

restart:
		// clear int status
		tx[0] = 0x42;
		tx[1] = 0xFF;
		transfer(fd, tx, rx, 2);

		// set config2
		tx[0] = 0x41;
		tx[1] = 0x40;
		transfer(fd, tx, rx, 2);

		// set config1
		tx[0] = 0x40;
		// measurement mode 1
		tx[1] = 0x01;
		// measurement mode 2
		// tx[1] = 0x03;
		transfer(fd, tx, rx, 2);

		// wait for INTB
		while (digitalRead(PIN_INTB) == 1);

		// read int status
		tx[0] = 0x02;
		tx[1] = 0x00;
		transfer(fd, tx, rx, 2);
		if (rx[1] != 0x19) {
			printf("int_status: %x\n", rx[1]);
			goto restart;
		}

		// read calibration1
		tx[0] = 0x1b;
		tx[1] = 0x00;
		tx[2] = 0x00;
		tx[3] = 0x00;
		transfer(fd, tx, rx, 4);
		uint32_t calibration1 = get23(rx);

		// read calibration2
		tx[0] = 0x1c;
		tx[1] = 0x00;
		tx[2] = 0x00;
		tx[3] = 0x00;
		transfer(fd, tx, rx, 4);
		uint32_t calibration2 = get23(rx);

		// read time1
		tx[0] = 0x10;
		tx[1] = 0x00;
		tx[2] = 0x00;
		tx[3] = 0x00;
		transfer(fd, tx, rx, 4);
		uint32_t time1 = get23(rx);

		double calibration2_periods = 10;
		double calCount = (double)(calibration2 - calibration1) / (double)(calibration2_periods - 1);
		double normLSB = CLOCKperiod / calCount;
		double tof = time1 * normLSB;

		m[num_valid_measurements] = tof;
		num_valid_measurements++;

		//printf("%03d/%03d - tof: %.2lf ns\n", num_valid_measurements, num_measurements, tof);

	}

	double sum = 0.0;
	for (int i = 0; i < num_valid_measurements; i++) {
		sum += m[i];
	}

	double mean = sum / num_valid_measurements;

	double variance = 0.0;
	for (int i = 0; i < num_valid_measurements; i++) {
		variance += ( (m[i] - mean) * (m[i] - mean) );
	}
	variance /= num_valid_measurements;

	double sigma = sqrt(variance);

	double min = 100000;
	double max = -1.0;

	for (int i = 0; i < num_valid_measurements; i++) {
		if (m[i] < min) min = m[i];
		if (m[i] > max) max = m[i];
	}

	qsort(m, num_valid_measurements, sizeof(double), comp);
	double median = m[num_valid_measurements/2];

	printf("# of measurements: %d\n", num_valid_measurements);
	printf("mean: %.2lf ns\n", mean);
	printf("sigma: %.2lf ns\n", sigma);
	printf("min: %.2lf ns\n", min);
	printf("max: %.2lf ns\n", max);
	printf("median: %.2lf ns\n", median);

	// disable TDC
	digitalWrite(PIN_ENABLE, 0);

	close(fd);

	return ret;
}


all: dut tia

dut: dut.c
	gcc -O2 -o dut dut.c -lwiringPi

tia: tia.c
	gcc -O2 -o tia tia.c -lwiringPi -lm

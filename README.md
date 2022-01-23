
# tdc7200_demo

This is just a basic demo, the programs are not implemented 100% correct, they are not intended to be re-used somewhere else.

Just do a make to compile the programs. wiringPi library is required.

Start dut first, then tia. tia can be run multiple times without stopping dut.

## tia (Time Interval Analyzer)

Performs 10000 measurements, each less than 500ns (measurement mode 1 of TDC7200).

For each measurement:

- starts measurement (writes to config1) (which automatically sets TRIGG to 1)
- waits for INTB to be 0
- calculates the time interval and stores the result

After all the measurements, statistics are calculated and displayed. 

Example output:

```
$ ./tia
# of measurements: 10000
mean: 31.75 ns
sigma: 4.26 ns
min: 7.82 ns
max: 227.36 ns
median: 34.59 ns
```

Constants, all hardcoded in tia:

- CLOCK is 5 MHz.
- spidev is /dev/spidev0.0.
- SPI speed is 2 MHz.

## dut (device under test)

In an infinite loop, waits for TRIGG to be 1 and then:

- sets START to 1
- sets STOP to 1
- sets START to 0
- sets STOP to 0

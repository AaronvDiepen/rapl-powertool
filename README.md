# Ryzen RAPL powertool
Power consumption analysis tool that uses rapl counters on the AMD Ryzen platform.

Reads periodically reads the average power consumption per cpu package over a time interval (in Watts) and outputs them as csv values.

Use --help for usage information.

## Compilation

gcc -o ryzen-rapl-powertool ryzen-rapl-powertool.c -lm

Needs access to /dev/cpu/CPUNO/msr files. So may need root rights

## Tested on: ##
 * AMD Ryzen™ 3 5300U


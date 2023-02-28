# RAPL powertool
Power consumption analysis tool that uses rapl counters on the AMD Ryzen platform and INTEL platform.

Periodically reads the average power consumption (in Watts) per cpu package over a time interval (in milliseconds) and outputs them as csv values.

Has an alternative mode to output the total energy consumption (in Joules) over a specific duration (in milliseconds).

Use --help for usage information.

## Compilation

gcc -o rapl-powertool rapl-powertool.c -lm

Needs access to /dev/cpu/CPUNO/msr files. So may need root rights

## Tested on:
 * AMD Ryzen™ 3 5300U
 * INTEL i7-7700HQ


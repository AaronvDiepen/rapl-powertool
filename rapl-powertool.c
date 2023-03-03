/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * This program is based on rapl-read-ryzen and intel_rapl_power:
 * https://github.com/djselbeck/rapl-read-ryzen
 * https://github.com/razvanlupusoru/Intel-RAPL-via-Sysfs
 * 
 * compile with:
 * gcc -o rapl-powertool rapl-powertool.c -lm
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>

/* The metadata of the program  */
#define PROGRAM_NAME            "RAPL powertool"
#define PROGRAM_VERSION         "v0.1"
#define AUTHOR                  "Aaron van Diepen"

/* Interval in milliseconds.    */
#define DEFAULT_INTERVAL         1000

/* AMD READ VALUES              */
#define AMD_MSR_PWR_UNIT         0xC0010299
#define AMD_MSR_PACKAGE_ENERGY   0xC001029B
#define INTEL_MSR_PWR_UNIT       0x606
#define INTEL_MSR_PACKAGE_ENERGY 0x611
#define TIME_UNIT_OFFSET         0x10
#define TIME_UNIT_MASK           0xF0000
#define ENERGY_UNIT_OFFSET       0x08
#define ENERGY_UNIT_MASK         0x1F00
#define POWER_UNIT_OFFSET        0x0
#define POWER_UNIT_MASK          0xF

/* MAX CPU VALUES              */
#define MAX_CORES               1024
#define MAX_PACKAGES            16

extern const char *__progname;

static uint64_t total_cores=0,total_packages=0;
static char* exec_name;

static int8_t package_map[MAX_PACKAGES];

static uint32_t MSR_PWR_UNIT;
static uint32_t MSR_PACKAGE_ENERGY;

static void
detect_packages (void)
{
  char filename[BUFSIZ];
  FILE *fff;
  int package;
  int i;

  for (i=0; i<MAX_PACKAGES; i++) package_map[i] = -1;

  for (i=0; i<MAX_CORES; i++)
  {
    sprintf(filename,"/sys/devices/system/cpu/cpu%d/topology/physical_package_id",i);
    fff=fopen(filename,"r");
    if (fff==NULL) break;
    fscanf(fff,"%d",&package);
    fclose(fff);

    if (package_map[package]==-1) {
      total_packages++;
        package_map[package]=i;
    }
  }

  total_cores=i;

  return;
}

static int
open_msr (uint64_t core)
{
  char msr_filename[BUFSIZ];
  int fd;

  sprintf(msr_filename, "/dev/cpu/%d/msr", core);
  fd = open(msr_filename, O_RDONLY);
  if (fd < 0)
  {
    if (errno == ENXIO)
    {
      fprintf(stderr, "\
rdmsr: No CPU %d\n\
",
             core
             );
      exit(2);
    }
    else if (errno == EIO)
    {
      fprintf(stderr, "\
rdmsr: CPU %d doesn't support MSRs\n\
",
             core
             );
      exit(3);
    }
    else
    {
      perror("rdmsr:open");
      fprintf(stderr,"\
Trying to open %s\n\
",
             msr_filename
             );
      exit(127);
    }
  }

  return fd;
}

static uint64_t
read_msr (int fd, uint64_t which)
{
  uint64_t data;

  if (pread(fd, &data, sizeof data, which) != sizeof data) {
    perror("rdmsr:pread");
    exit(127);
  }

  return data;
}

static void
rapl_msr_package_interval (uint64_t interval)
{
  uint64_t i;
  double energy_used;
  double *time_unit = (double*)malloc(sizeof(double)*total_packages);
  double *energy_unit = (double*)malloc(sizeof(double)*total_packages);
  double *power_unit = (double*)malloc(sizeof(double)*total_packages);
  double *prev_energy_used = (double*)malloc(sizeof(double)*total_packages);

  int *fd = (int*)malloc(sizeof(int)*total_packages);

  for (i = 0; i < total_packages; i++) {
    fd[i] = open_msr(package_map[i]);

    uint64_t core_energy_units = read_msr(fd[i], MSR_PWR_UNIT);

    time_unit[i] = pow (0.5, (double)((core_energy_units & TIME_UNIT_MASK) >> TIME_UNIT_OFFSET));
    energy_unit[i] = pow (0.5,(double)((core_energy_units & ENERGY_UNIT_MASK) >> ENERGY_UNIT_OFFSET));
    power_unit[i] = pow (0.5,(double)((core_energy_units & POWER_UNIT_MASK) >> POWER_UNIT_OFFSET));

    prev_energy_used[i] = read_msr (fd[i], MSR_PACKAGE_ENERGY);
  }

  while (1) {
    usleep(interval * 1000);
    for (i = 0; i < total_packages;i++)
    {
      energy_used = read_msr(fd[i], MSR_PACKAGE_ENERGY);
      if (i > 0)
      {
        fputs(",", stdout);
      }

      uint64_t energy_diff = energy_used - prev_energy_used[i];
      if (energy_used < prev_energy_used[i]) {
        energy_diff = 0xFFFFFFFF - energy_diff;
      }

      printf("%g",
              energy_diff * energy_unit[i] * 1000 / interval
            );

      prev_energy_used[i] = energy_used;
    }
    fputs("\n", stdout);
  }

  return;
}

static void
rapl_msr_package_duration (uint64_t duration)
{
  uint64_t i;
  double energy_used;
  double *time_unit = (double*)malloc(sizeof(double)*total_packages);
  double *energy_unit = (double*)malloc(sizeof(double)*total_packages);
  double *power_unit = (double*)malloc(sizeof(double)*total_packages);
  double *prev_energy_used = (double*)malloc(sizeof(double)*total_packages);

  int *fd = (int*)malloc(sizeof(int)*total_packages);

  for (i = 0; i < total_packages; i++) {
    fd[i] = open_msr(package_map[i]);

    uint64_t core_energy_units = read_msr(fd[i], MSR_PWR_UNIT);

    time_unit[i] = pow (0.5, (double)((core_energy_units & TIME_UNIT_MASK) >> TIME_UNIT_OFFSET));
    energy_unit[i] = pow (0.5,(double)((core_energy_units & ENERGY_UNIT_MASK) >> ENERGY_UNIT_OFFSET));
    power_unit[i] = pow (0.5,(double)((core_energy_units & POWER_UNIT_MASK) >> POWER_UNIT_OFFSET));

    prev_energy_used[i] = read_msr (fd[i], MSR_PACKAGE_ENERGY);
  }

  usleep(duration * 1000);
  for (i = 0; i < total_packages;i++)
  {
    energy_used = read_msr(fd[i], MSR_PACKAGE_ENERGY);
    if (i > 0)
    {
      fputs(",", stdout);
    }

    uint64_t energy_diff = energy_used - prev_energy_used[i];
    if (energy_used < prev_energy_used[i]) {
      energy_diff = 0xFFFFFFFF - energy_diff;
    }

    printf("%g",
            energy_diff * energy_unit[i]
          );
  }
  fputs("\n", stdout);

  return;
}

static inline void
emit_try_help (void)
{
  fprintf (stderr, "\
Try '%s --help' for more information.\n\
",
          __progname
          );
}

static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf ("\
Usage: %s [AMD/INTEL] [OPTION]... \n\
\n\
",
             __progname
             );

      printf ("\
Measure average cpu power usage every %d ms, outputs in Watt\n\
or total energy consumption over a duration of [d] ms, outputs in Joules\n\
uses RAPL energy status to perform the measurements.\n\
\n\
Outputs as csv when multiple packages are detected\n\
",
             DEFAULT_INTERVAL
             );

      printf ("\
  -i, --interval    measure every [VAL] ms, instead of every %d ms\n\
",
             DEFAULT_INTERVAL
             );

      fputs ("\
  -d, --duration    measure over [VAL] ms, outputs the consumption in Joules\n\
      --help        display this help and exit\n\
      --version     output version information and exit\n\
", stdout);
    }
  exit (status);
}

static void
version_etc ()
{
  printf ("\
%s %s \n\
",
         PROGRAM_NAME,
         PROGRAM_VERSION
         );
  fputs ("\
Copyright (C) 2022 Free Software Foundation, Inc.\n\
License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\
\n\
", stdout);
  printf ("\
Written by %s.\n\
",
         AUTHOR
         );
}

static uint64_t
convert (char *st)
{
  char *x;
  for (x = st ; *x ; x++) {
    if (!isdigit(*x))
      usage (EXIT_FAILURE);
  }
  return (strtoul(st, NULL, 10));
}

int
main (int argc, char **argv)
{
  /* Variables that are set according to the specified options.  */
  uint64_t interval = DEFAULT_INTERVAL;
  uint64_t duration = 0;
  bool cpu_specified = false;

  if (argc < 2)
  {
    usage (EXIT_FAILURE);
  }
  else if (!strcmp(argv[1], "AMD"))
  {
    MSR_PWR_UNIT = AMD_MSR_PWR_UNIT;
    MSR_PACKAGE_ENERGY = AMD_MSR_PACKAGE_ENERGY;
    cpu_specified = true;
    argv++;
    argc--;
  }
  else if (!strcmp(argv[1], "INTEL"))
  {
    MSR_PWR_UNIT = INTEL_MSR_PWR_UNIT;
    MSR_PACKAGE_ENERGY = INTEL_MSR_PACKAGE_ENERGY;
    cpu_specified = true;
    argv++;
    argc--;
  }

  static struct option const long_options[] =
  {
    {"interval", required_argument, NULL, 'i'},
    {"duration", required_argument, NULL, 'd'},
    {"help", no_argument, NULL, -2},
    {"version", no_argument, NULL, -3},
    {NULL, 0, NULL, 0}
  };

  char c;
  while ((c = getopt_long (argc, argv, "i:d:", long_options, NULL))
      != -1)
  {
    switch (c)
    {
      case 'i':
        interval = convert (optarg);
        break;

      case 'd':
        duration = convert (optarg);
        break;

      case -2:
        usage (EXIT_SUCCESS);
        break;

      case -3:
        version_etc ();
        exit (EXIT_SUCCESS);
        break;

      default:
        usage (EXIT_FAILURE);
    }
  }

  if (!cpu_specified) {
    usage (EXIT_FAILURE);
  }

  detect_packages ();

  if (duration > 0) {
    rapl_msr_package_duration (duration);
    exit (EXIT_SUCCESS);
  }

  rapl_msr_package_interval (interval);

  exit (EXIT_SUCCESS);
}

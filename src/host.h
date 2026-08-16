#ifndef ROOKIETOP_HOST_H
#define ROOKIETOP_HOST_H

#include <stdio.h>

#define HOST_NAME_MAX 64
#define HOST_KERNEL_MAX 64
#define HOST_THERMAL_TYPE_MAX 64

struct host_info {
    char hostname[HOST_NAME_MAX];
    char kernel_release[HOST_KERNEL_MAX];
    char thermal_type[HOST_THERMAL_TYPE_MAX];
    long cores;
    double uptime_seconds;
    double load1;
    double load5;
    double load15;
    double thermal_c;
    int thermal_ok;
};

int host_parse_uptime(FILE *file, double *out_seconds);
int host_parse_loadavg(FILE *file, double *out_load1, double *out_load5, double *out_load15);
int host_read(struct host_info *out);

#endif

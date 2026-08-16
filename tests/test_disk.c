#include "disk.h"

#include <math.h>
#include <stdio.h>

static int close_enough(double a, double b)
{
    return fabs(a - b) < 0.0001;
}

int main(void)
{
    struct disk_sample sample = {.total_bytes = 1000, .available_bytes = 250};
    double percent;
    uint64_t used;

    if (disk_usage(&sample, &percent, &used) != 0 ||
        used != 750 || !close_enough(percent, 75.0)) {
        return 1;
    }

    sample.available_bytes = 1001;
    if (disk_usage(&sample, &percent, &used) == 0) {
        return 1;
    }

    sample.total_bytes = 0;
    sample.available_bytes = 0;
    if (disk_usage(&sample, &percent, &used) == 0) {
        return 1;
    }

    puts("disk tests passed");
    return 0;
}

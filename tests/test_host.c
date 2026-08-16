#include "host.h"

#include <math.h>
#include <stdio.h>

static int near(double a, double b)
{
    return fabs(a - b) < 0.001;
}

int main(void)
{
    FILE *file = tmpfile();
    if (file == NULL) {
        return 1;
    }

    fputs("12345.67 999.00\n", file);
    rewind(file);
    double uptime;
    if (host_parse_uptime(file, &uptime) != 0 || !near(uptime, 12345.67)) {
        fclose(file);
        return 1;
    }
    fclose(file);

    file = tmpfile();
    if (file == NULL) {
        return 1;
    }

    fputs("0.42 0.35 0.27 2/150 1234\n", file);
    rewind(file);
    double load1;
    double load5;
    double load15;
    if (host_parse_loadavg(file, &load1, &load5, &load15) != 0 ||
        !near(load1, 0.42) || !near(load5, 0.35) || !near(load15, 0.27)) {
        fclose(file);
        return 1;
    }
    fclose(file);

    file = tmpfile();
    if (file == NULL) {
        return 1;
    }
    fputs("not-a-load\n", file);
    rewind(file);
    if (host_parse_loadavg(file, &load1, &load5, &load15) == 0) {
        fclose(file);
        return 1;
    }
    fclose(file);

    puts("host tests passed");
    return 0;
}

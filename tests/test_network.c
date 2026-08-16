#include "network.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int close_enough(double a, double b)
{
    return fabs(a - b) < 0.0001;
}

int main(void)
{
    const char *line = "  eth0: 1000 10 0 0 0 0 0 0 500 5 0 0 0 0 0 0\n";
    char name[16];
    uint64_t rx;
    uint64_t tx;

    if (network_parse_line(line, name, sizeof(name), &rx, &tx) != 0 ||
        strcmp(name, "eth0") != 0 || rx != 1000 || tx != 500) {
        return 1;
    }

    if (network_parse_line("bad line\n", name, sizeof(name), &rx, &tx) == 0) {
        return 1;
    }

    struct network_sample prev = {.rx_bytes = 1000, .tx_bytes = 500};
    struct network_sample curr = {.rx_bytes = 1500, .tx_bytes = 750};
    double rx_rate;
    double tx_rate;

    if (network_rate(&prev, &curr, 0.5, &rx_rate, &tx_rate) != 0 ||
        !close_enough(rx_rate, 1000.0) || !close_enough(tx_rate, 500.0)) {
        return 1;
    }

    curr.rx_bytes = 900;
    if (network_rate(&prev, &curr, 0.5, &rx_rate, &tx_rate) == 0) {
        return 1;
    }

    puts("network tests passed");
    return 0;
}

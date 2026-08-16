#include "process_cpu.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *line =
        "4242 (worker thread) S 1 2 3 4 5 6 7 8 9 10 120 30 0 0 20 0 1 0 999 0 0 0\n";

    struct process_cpu_stat stat;
    if (process_cpu_parse_stat(line, &stat) != 0 ||
        stat.pid != 4242 || strcmp(stat.name, "worker thread") != 0 ||
        stat.ticks != 150 || stat.starttime != 999) {
        return 1;
    }

    const char *parenthesis_name =
        "7 (worker) odd) R 1 2 3 4 5 6 7 8 9 10 5 6 0 0 20 0 1 0 77 0 0\n";
    if (process_cpu_parse_stat(parenthesis_name, &stat) != 0 ||
        stat.pid != 7 || strcmp(stat.name, "worker) odd") != 0 ||
        stat.ticks != 11 || stat.starttime != 77) {
        return 1;
    }

    if (process_cpu_parse_stat("broken", &stat) == 0) {
        return 1;
    }

    puts("process CPU tests passed");
    return 0;
}

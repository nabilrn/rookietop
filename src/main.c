#define _POSIX_C_SOURCE 200809L

#include "cpu.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define ROOKIETOP_VERSION "0.1.0-dev"
#define CPU_SAMPLE_NS 250000000L

static void print_help(void)
{
    puts("RookieTop - beginner-first Linux system monitor");
    puts("");
    puts("Usage: rookietop [OPTION]");
    puts("");
    puts("Options:");
    puts("  -h, --help       Show this help");
    puts("  -V, --version    Show version");
}

static int wait_for_next_sample(void)
{
    struct timespec delay = {.tv_sec = 0, .tv_nsec = CPU_SAMPLE_NS};

    while (nanosleep(&delay, &delay) != 0) {
        if (errno != EINTR) {
            return -1;
        }
    }

    return 0;
}

static const char *cpu_status(double usage)
{
    if (usage >= 90.0) {
        return "High";
    }
    if (usage >= 70.0) {
        return "Busy";
    }
    return "Normal";
}

static void print_cpu_explanation(double usage)
{
    if (usage >= 90.0) {
        puts("Your CPU is working very hard right now.");
    } else if (usage >= 70.0) {
        puts("Your CPU is busy, but this can be normal during active work.");
    } else {
        puts("Your CPU has plenty of room right now.");
    }

    puts("RookieTop calculates this from two /proc/stat samples.");
}

static int show_cpu(void)
{
    struct cpu_sample prev;
    struct cpu_sample curr;
    double usage;

    if (cpu_read(&prev) != 0 || wait_for_next_sample() != 0 || cpu_read(&curr) != 0) {
        fputs("rookietop: could not read CPU data from /proc/stat\n", stderr);
        return 1;
    }

    if (cpu_usage(&prev, &curr, &usage) != 0) {
        fputs("rookietop: CPU counters changed unexpectedly\n", stderr);
        return 1;
    }

    puts("RookieTop");
    printf("CPU  %5.1f%%  %s\n", usage, cpu_status(usage));
    print_cpu_explanation(usage);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        return show_cpu();
    }

    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_help();
        return 0;
    }

    if (argc == 2 && (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0)) {
        puts(ROOKIETOP_VERSION);
        return 0;
    }

    fputs("rookietop: unknown option\n", stderr);
    return 2;
}

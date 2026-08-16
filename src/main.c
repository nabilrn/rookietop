#define _POSIX_C_SOURCE 200809L

#include "cpu.h"
#include "memory.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define ROOKIETOP_VERSION "0.2.0-dev"
#define CPU_SAMPLE_NS 250000000L
#define KIB_PER_GIB 1048576.0

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

static const char *memory_status(double usage)
{
    if (usage >= 90.0) {
        return "Attention";
    }
    if (usage >= 80.0) {
        return "Busy";
    }
    return "Normal";
}

static void print_cpu_explanation(double usage)
{
    if (usage >= 90.0) {
        puts("CPU: Your CPU is working very hard right now.");
    } else if (usage >= 70.0) {
        puts("CPU: Your CPU is busy, but this can be normal during active work.");
    } else {
        puts("CPU: Your CPU has plenty of room right now.");
    }

    puts("     Usage is derived from two /proc/stat samples.");
}

static void print_memory_explanation(double usage, uint64_t swap_used_kib)
{
    if (usage >= 90.0) {
        puts("Memory: Available memory is getting low; sustained pressure may slow the system.");
    } else if (usage >= 80.0) {
        puts("Memory: Most memory is in use, but Linux can still reclaim cache for programs.");
    } else {
        puts("Memory: Memory availability looks comfortable.");
    }

    puts("        RookieTop uses MemAvailable, so filesystem cache is not treated as wasted RAM.");
    if (swap_used_kib > 0) {
        puts("        Some swap is in use; that alone does not mean the system is unhealthy.");
    }
}

static int show_overview(void)
{
    struct cpu_sample prev;
    struct cpu_sample curr;
    struct memory_sample memory;
    double cpu_percent;
    double memory_percent;
    uint64_t memory_used_kib;
    uint64_t swap_used_kib;

    if (cpu_read(&prev) != 0 || memory_read(&memory) != 0 ||
        wait_for_next_sample() != 0 || cpu_read(&curr) != 0) {
        fputs("rookietop: could not read system data from /proc\n", stderr);
        return 1;
    }

    if (cpu_usage(&prev, &curr, &cpu_percent) != 0 ||
        memory_usage(&memory, &memory_percent, &memory_used_kib, &swap_used_kib) != 0) {
        fputs("rookietop: kernel counters changed unexpectedly\n", stderr);
        return 1;
    }

    puts("RookieTop");
    printf("CPU     %5.1f%%  %s\n", cpu_percent, cpu_status(cpu_percent));
    printf("Memory  %5.1f%%  %s\n", memory_percent, memory_status(memory_percent));
    printf("        %.1f / %.1f GiB used, %.1f GiB available\n",
           (double)memory_used_kib / KIB_PER_GIB,
           (double)memory.total_kib / KIB_PER_GIB,
           (double)memory.available_kib / KIB_PER_GIB);
    printf("Swap    %.1f / %.1f GiB used\n",
           (double)swap_used_kib / KIB_PER_GIB,
           (double)memory.swap_total_kib / KIB_PER_GIB);
    puts("");
    print_cpu_explanation(cpu_percent);
    print_memory_explanation(memory_percent, swap_used_kib);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        return show_overview();
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

#define _POSIX_C_SOURCE 200809L

#include "cpu.h"
#include "disk.h"
#include "memory.h"
#include "network.h"
#include "process.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ROOKIETOP_VERSION "0.1.0-alpha.1"
#define SAMPLE_NS 250000000L
#define REFRESH_NS 750000000L
#define KIB_PER_GIB 1048576.0
#define KIB_PER_MIB 1024.0
#define BYTES_PER_GIB 1073741824.0
#define BYTES_PER_MIB 1048576.0
#define TOP_PROCESS_COUNT 3

static void print_help(void)
{
    puts("RookieTop - beginner-first Linux system monitor");
    puts("");
    puts("Usage: rookietop [OPTION]");
    puts("");
    puts("Options:");
    puts("  --once           Print one snapshot and exit");
    puts("  -h, --help       Show this help");
    puts("  -V, --version    Show version");
}

static int wait_ns(long nanoseconds)
{
    struct timespec delay = {.tv_sec = 0, .tv_nsec = nanoseconds};

    while (nanosleep(&delay, &delay) != 0) {
        if (errno != EINTR) {
            return -1;
        }
    }

    return 0;
}

static double elapsed_seconds(const struct timespec *start, const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
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

static const char *disk_status(double usage)
{
    if (usage >= 95.0) {
        return "Critical";
    }
    if (usage >= 85.0) {
        return "Attention";
    }
    return "Normal";
}

static const char *overall_status(double cpu, double memory, double disk)
{
    if (disk >= 95.0) {
        return "CRITICAL";
    }
    if (cpu >= 90.0 || memory >= 90.0 || disk >= 85.0) {
        return "ATTENTION";
    }
    return "HEALTHY";
}

static void print_explanations(double cpu,
                               double memory,
                               double disk,
                               uint64_t swap_used_kib)
{
    puts("");
    puts("What this means");

    if (cpu >= 90.0) {
        puts("  CPU: working very hard right now; sustained high usage can make the system feel slow.");
    } else if (cpu >= 70.0) {
        puts("  CPU: busy, which can be normal during active work.");
    } else {
        puts("  CPU: plenty of processing room right now.");
    }

    if (memory >= 90.0) {
        puts("  RAM: available memory is low; sustained pressure can cause swapping and slowdown.");
    } else if (memory >= 80.0) {
        puts("  RAM: most memory is in use, but Linux can still reclaim filesystem cache.");
    } else {
        puts("  RAM: available memory looks comfortable.");
    }

    if (disk >= 95.0) {
        puts("  Disk: root storage is almost full; free space soon.");
    } else if (disk >= 85.0) {
        puts("  Disk: root storage is getting full.");
    } else {
        puts("  Disk: root storage has comfortable free space.");
    }

    if (swap_used_kib > 0) {
        puts("  Swap: some swap is used; that alone does not mean the system is unhealthy.");
    }
}

static int show_overview(int clear_screen, int live)
{
    struct cpu_sample cpu_prev;
    struct cpu_sample cpu_curr;
    struct memory_sample memory;
    struct disk_sample disk;
    struct network_sample net_prev;
    struct network_sample net_curr;
    struct timespec net_start;
    struct timespec net_end;
    double cpu_percent;
    double memory_percent;
    double disk_percent;
    double rx_per_sec = 0.0;
    double tx_per_sec = 0.0;
    uint64_t memory_used_kib;
    uint64_t swap_used_kib;
    uint64_t disk_used_bytes;
    int network_ok = 1;

    if (memory_read(&memory) != 0 || disk_read_root(&disk) != 0 || cpu_read(&cpu_prev) != 0) {
        fputs("rookietop: could not read system data\n", stderr);
        return 1;
    }

    if (network_read(&net_prev) != 0 || clock_gettime(CLOCK_MONOTONIC, &net_start) != 0) {
        network_ok = 0;
    }

    if (wait_ns(SAMPLE_NS) != 0 || cpu_read(&cpu_curr) != 0) {
        fputs("rookietop: could not sample CPU data\n", stderr);
        return 1;
    }

    if (network_ok &&
        (network_read(&net_curr) != 0 || clock_gettime(CLOCK_MONOTONIC, &net_end) != 0 ||
         network_rate(&net_prev, &net_curr, elapsed_seconds(&net_start, &net_end),
                      &rx_per_sec, &tx_per_sec) != 0)) {
        network_ok = 0;
    }

    if (cpu_usage(&cpu_prev, &cpu_curr, &cpu_percent) != 0 ||
        memory_usage(&memory, &memory_percent, &memory_used_kib, &swap_used_kib) != 0 ||
        disk_usage(&disk, &disk_percent, &disk_used_bytes) != 0) {
        fputs("rookietop: kernel counters changed unexpectedly\n", stderr);
        return 1;
    }

    struct process_info process_top[TOP_PROCESS_COUNT];
    size_t process_count = 0;
    size_t process_total = 0;
    int process_ok = process_top_memory(process_top,
                                        TOP_PROCESS_COUNT,
                                        &process_count,
                                        &process_total) == 0;

    if (clear_screen) {
        fputs("\033[H\033[2J", stdout);
    }

    printf("RookieTop %s\n", ROOKIETOP_VERSION);
    printf("System health: %s\n", overall_status(cpu_percent, memory_percent, disk_percent));
    puts("----------------------------------------");
    printf("CPU      %5.1f%%  %s\n", cpu_percent, cpu_status(cpu_percent));
    printf("Memory   %5.1f%%  %s\n", memory_percent, memory_status(memory_percent));
    printf("         %.1f / %.1f GiB used | %.1f GiB available\n",
           (double)memory_used_kib / KIB_PER_GIB,
           (double)memory.total_kib / KIB_PER_GIB,
           (double)memory.available_kib / KIB_PER_GIB);
    printf("Swap     %.1f / %.1f GiB used\n",
           (double)swap_used_kib / KIB_PER_GIB,
           (double)memory.swap_total_kib / KIB_PER_GIB);
    printf("Disk     %5.1f%%  %s\n", disk_percent, disk_status(disk_percent));
    printf("         %.1f / %.1f GiB used | %.1f GiB available\n",
           (double)disk_used_bytes / BYTES_PER_GIB,
           (double)disk.total_bytes / BYTES_PER_GIB,
           (double)disk.available_bytes / BYTES_PER_GIB);

    if (network_ok) {
        printf("Network  down %.2f MiB/s | up %.2f MiB/s\n",
               rx_per_sec / BYTES_PER_MIB,
               tx_per_sec / BYTES_PER_MIB);
    } else {
        puts("Network  unavailable for this sample");
    }

    puts("");
    if (process_ok) {
        printf("Top memory processes (%zu readable processes)\n", process_total);
        puts("  PID      RSS MiB  NAME");
        for (size_t i = 0; i < process_count; i++) {
            printf("  %-8d %7.1f  %s\n",
                   process_top[i].pid,
                   (double)process_top[i].rss_kib / KIB_PER_MIB,
                   process_top[i].name);
        }
    } else {
        puts("Top memory processes unavailable");
    }

    print_explanations(cpu_percent, memory_percent, disk_percent, swap_used_kib);
    puts("");
    puts("Sources: /proc/stat | /proc/meminfo | /proc/net/dev | /proc/<pid>/status | statvfs()");
    if (live) {
        puts("Live mode: refresh ~1s | Ctrl+C to quit");
    }

    return 0;
}

static int run_monitor(int force_once)
{
    int live = !force_once && isatty(STDOUT_FILENO);

    if (!live) {
        return show_overview(0, 0);
    }

    for (;;) {
        int result = show_overview(1, 1);
        if (result != 0) {
            return result;
        }
        fflush(stdout);

        if (wait_ns(REFRESH_NS) != 0) {
            return 1;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        return run_monitor(0);
    }

    if (argc == 2 && strcmp(argv[1], "--once") == 0) {
        return run_monitor(1);
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

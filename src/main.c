#define _POSIX_C_SOURCE 200809L

#include "cpu.h"
#include "disk.h"
#include "memory.h"
#include "network.h"
#include "process.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ROOKIETOP_VERSION "0.1.0-alpha.2"
#define SAMPLE_NS 250000000L
#define REFRESH_NS 750000000L
#define KIB_PER_GIB 1048576.0
#define KIB_PER_MIB 1024.0
#define BYTES_PER_GIB 1073741824.0
#define BYTES_PER_MIB 1048576.0
#define TOP_PROCESS_COUNT 3
#define BAR_WIDTH 18

#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_DIM "\033[2m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED "\033[31m"

static volatile sig_atomic_t stop_requested = 0;
static int use_color = 0;
static int cursor_hidden = 0;

static const char *ansi(const char *code)
{
    return use_color ? code : "";
}

static void handle_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static void restore_cursor(void)
{
    if (cursor_hidden) {
        fputs("\033[?25h", stdout);
        fflush(stdout);
        cursor_hidden = 0;
    }
}

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
    puts("");
    puts("Set NO_COLOR to disable ANSI colors.");
}

static int wait_ns(long nanoseconds)
{
    struct timespec delay = {.tv_sec = 0, .tv_nsec = nanoseconds};

    while (nanosleep(&delay, &delay) != 0) {
        if (errno == EINTR && stop_requested) {
            return 0;
        }
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

static const char *cpu_color(double usage)
{
    return usage >= 70.0 ? ANSI_YELLOW : ANSI_GREEN;
}

static const char *memory_color(double usage)
{
    return usage >= 80.0 ? ANSI_YELLOW : ANSI_GREEN;
}

static const char *disk_color(double usage)
{
    if (usage >= 95.0) {
        return ANSI_RED;
    }
    return usage >= 85.0 ? ANSI_YELLOW : ANSI_GREEN;
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

static const char *overall_color(double cpu, double memory, double disk)
{
    if (disk >= 95.0) {
        return ANSI_RED;
    }
    if (cpu >= 90.0 || memory >= 90.0 || disk >= 85.0) {
        return ANSI_YELLOW;
    }
    return ANSI_GREEN;
}

static void print_rule(void)
{
    puts("------------------------------------------------------------");
}

static void print_section(const char *name)
{
    printf("%s%s%s\n", ansi(ANSI_BOLD), name, ansi(ANSI_RESET));
}

static void print_bar(double percent, const char *color)
{
    int filled = (int)(percent * (double)BAR_WIDTH / 100.0 + 0.5);

    if (filled < 0) {
        filled = 0;
    }
    if (filled > BAR_WIDTH) {
        filled = BAR_WIDTH;
    }

    putchar('[');
    fputs(ansi(color), stdout);
    for (int i = 0; i < filled; i++) {
        putchar('#');
    }
    fputs(ansi(ANSI_RESET), stdout);
    for (int i = filled; i < BAR_WIDTH; i++) {
        putchar('.');
    }
    putchar(']');
}

static void print_metric(const char *name,
                         double percent,
                         const char *status,
                         const char *color)
{
    printf("%-8s ", name);
    print_bar(percent, color);
    printf(" %5.1f%%  %s%s%s\n",
           percent,
           ansi(color),
           status,
           ansi(ANSI_RESET));
}

static void print_insight(double cpu,
                          double memory,
                          double disk,
                          uint64_t swap_used_kib)
{
    print_section("INSIGHT");

    if (disk >= 95.0) {
        printf("%s!%s Root storage is almost full. Free space soon.\n",
               ansi(ANSI_RED), ansi(ANSI_RESET));
    } else if (memory >= 90.0) {
        printf("%s!%s Available memory is low. Sustained pressure can slow the system.\n",
               ansi(ANSI_YELLOW), ansi(ANSI_RESET));
    } else if (cpu >= 90.0) {
        printf("%s!%s CPU is working very hard right now.\n",
               ansi(ANSI_YELLOW), ansi(ANSI_RESET));
    } else if (disk >= 85.0) {
        printf("%s!%s Root storage is getting full; keep an eye on free space.\n",
               ansi(ANSI_YELLOW), ansi(ANSI_RESET));
    } else {
        printf("%sOK%s CPU, memory, and root storage all have comfortable headroom.\n",
               ansi(ANSI_GREEN), ansi(ANSI_RESET));
    }

    if (swap_used_kib > 0) {
        printf("%sNote:%s swap is in use; that alone does not mean the system is unhealthy.\n",
               ansi(ANSI_DIM), ansi(ANSI_RESET));
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
        fputs("\033[H", stdout);
    }

    printf("%sRookieTop%s  %s%s%s  ",
           ansi(ANSI_BOLD),
           ansi(ANSI_RESET),
           ansi(ANSI_DIM),
           ROOKIETOP_VERSION,
           ansi(ANSI_RESET));
    printf("%s%s%s\n",
           ansi(overall_color(cpu_percent, memory_percent, disk_percent)),
           overall_status(cpu_percent, memory_percent, disk_percent),
           ansi(ANSI_RESET));
    print_rule();

    print_section("SYSTEM");
    print_metric("CPU", cpu_percent, cpu_status(cpu_percent), cpu_color(cpu_percent));
    print_metric("Memory", memory_percent, memory_status(memory_percent), memory_color(memory_percent));
    print_metric("Disk", disk_percent, disk_status(disk_percent), disk_color(disk_percent));
    puts("");

    printf("RAM      %.1f / %.1f GiB used    %.1f GiB available\n",
           (double)memory_used_kib / KIB_PER_GIB,
           (double)memory.total_kib / KIB_PER_GIB,
           (double)memory.available_kib / KIB_PER_GIB);
    printf("Swap     %.1f / %.1f GiB used\n",
           (double)swap_used_kib / KIB_PER_GIB,
           (double)memory.swap_total_kib / KIB_PER_GIB);
    printf("Root     %.1f / %.1f GiB used    %.1f GiB available\n",
           (double)disk_used_bytes / BYTES_PER_GIB,
           (double)disk.total_bytes / BYTES_PER_GIB,
           (double)disk.available_bytes / BYTES_PER_GIB);

    if (network_ok) {
        printf("Network  down %.2f MiB/s          up %.2f MiB/s\n",
               rx_per_sec / BYTES_PER_MIB,
               tx_per_sec / BYTES_PER_MIB);
    } else {
        puts("Network  unavailable for this sample");
    }

    puts("");
    if (process_ok) {
        printf("%sTOP MEMORY%s  %s%zu readable processes%s\n",
               ansi(ANSI_BOLD),
               ansi(ANSI_RESET),
               ansi(ANSI_DIM),
               process_total,
               ansi(ANSI_RESET));
        puts("NAME                      RSS MiB      PID");
        for (size_t i = 0; i < process_count; i++) {
            printf("%-24s %7.1f  %8d\n",
                   process_top[i].name,
                   (double)process_top[i].rss_kib / KIB_PER_MIB,
                   process_top[i].pid);
        }
    } else {
        puts("TOP MEMORY  unavailable");
    }

    puts("");
    print_insight(cpu_percent, memory_percent, disk_percent, swap_used_kib);
    puts("");
    printf("%sSources: /proc/stat | /proc/meminfo | /proc/net/dev | /proc/<pid>/status | statvfs()%s\n",
           ansi(ANSI_DIM), ansi(ANSI_RESET));
    if (live) {
        printf("%sLive ~1s  |  Ctrl+C quit%s\n", ansi(ANSI_DIM), ansi(ANSI_RESET));
    }

    if (clear_screen) {
        fputs("\033[J", stdout);
    }

    return 0;
}

static int run_monitor(int force_once)
{
    int terminal = isatty(STDOUT_FILENO);
    int live = !force_once && terminal;

    use_color = terminal && getenv("NO_COLOR") == NULL;

    if (!live) {
        return show_overview(0, 0);
    }

    if (signal(SIGINT, handle_stop) == SIG_ERR || signal(SIGTERM, handle_stop) == SIG_ERR) {
        fputs("rookietop: could not install signal handler\n", stderr);
        return 1;
    }

    fputs("\033[?25l\033[2J\033[H", stdout);
    cursor_hidden = 1;
    if (atexit(restore_cursor) != 0) {
        restore_cursor();
        fputs("rookietop: could not register terminal cleanup\n", stderr);
        return 1;
    }

    while (!stop_requested) {
        int result = show_overview(1, 1);
        if (result != 0) {
            return result;
        }
        fflush(stdout);

        if (stop_requested) {
            break;
        }
        if (wait_ns(REFRESH_NS) != 0) {
            return 1;
        }
    }

    return 0;
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

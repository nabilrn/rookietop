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
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define ROOKIETOP_VERSION "0.1.0-alpha.3"
#define SAMPLE_NS 250000000L
#define REFRESH_NS 750000000L
#define KIB_PER_GIB 1048576.0
#define KIB_PER_MIB 1024.0
#define BYTES_PER_GIB 1073741824.0
#define BYTES_PER_MIB 1048576.0
#define TOP_PROCESS_COUNT 3
#define COMPACT_BAR_WIDTH 18
#define MIN_FULL_COLS 80
#define MIN_FULL_ROWS 24

#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_DIM "\033[2m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED "\033[31m"

struct overview {
    struct memory_sample memory;
    struct disk_sample disk;
    struct process_info process_top[TOP_PROCESS_COUNT];
    double cpu_percent;
    double memory_percent;
    double disk_percent;
    double rx_per_sec;
    double tx_per_sec;
    uint64_t memory_used_kib;
    uint64_t swap_used_kib;
    uint64_t disk_used_bytes;
    size_t process_count;
    size_t process_total;
    int network_ok;
    int process_ok;
};

struct terminal_size {
    int rows;
    int cols;
};

static volatile sig_atomic_t stop_requested = 0;
static int use_color = 0;
static int terminal_active = 0;

static const char *ansi(const char *code)
{
    return use_color ? code : "";
}

static void handle_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static void restore_terminal(void)
{
    if (terminal_active) {
        fputs("\033[0m\033[?25h\033[?1049l", stdout);
        fflush(stdout);
        terminal_active = 0;
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
    puts("Interactive mode uses the full terminal screen.");
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

static struct terminal_size terminal_size(void)
{
    struct terminal_size size = {.rows = 24, .cols = 80};
    struct winsize window;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0) {
        if (window.ws_row > 0) {
            size.rows = (int)window.ws_row;
        }
        if (window.ws_col > 0) {
            size.cols = (int)window.ws_col;
        }
    }

    return size;
}

static void move_to(int row, int col)
{
    printf("\033[%d;%dH", row, col);
}

static void print_rule_width(int width)
{
    for (int i = 0; i < width; i++) {
        putchar('-');
    }
}

static void print_section(const char *name)
{
    printf("%s%s%s", ansi(ANSI_BOLD), name, ansi(ANSI_RESET));
}

static void print_bar(double percent, int width, const char *color)
{
    int filled = (int)(percent * (double)width / 100.0 + 0.5);

    if (filled < 0) {
        filled = 0;
    }
    if (filled > width) {
        filled = width;
    }

    putchar('[');
    fputs(ansi(color), stdout);
    for (int i = 0; i < filled; i++) {
        putchar('#');
    }
    fputs(ansi(ANSI_RESET), stdout);
    for (int i = filled; i < width; i++) {
        putchar('.');
    }
    putchar(']');
}

static void print_metric(const char *name,
                         double percent,
                         const char *status,
                         const char *color,
                         int bar_width)
{
    printf("%-8s ", name);
    print_bar(percent, bar_width, color);
    printf(" %5.1f%%  %s%s%s",
           percent,
           ansi(color),
           status,
           ansi(ANSI_RESET));
}

static void print_insight(const struct overview *view)
{
    if (view->disk_percent >= 95.0) {
        printf("%s!%s Root storage is almost full. Free space soon.",
               ansi(ANSI_RED), ansi(ANSI_RESET));
    } else if (view->memory_percent >= 90.0) {
        printf("%s!%s Available memory is low. Sustained pressure can slow the system.",
               ansi(ANSI_YELLOW), ansi(ANSI_RESET));
    } else if (view->cpu_percent >= 90.0) {
        printf("%s!%s CPU is working very hard right now.",
               ansi(ANSI_YELLOW), ansi(ANSI_RESET));
    } else if (view->disk_percent >= 85.0) {
        printf("%s!%s Root storage is getting full; keep an eye on free space.",
               ansi(ANSI_YELLOW), ansi(ANSI_RESET));
    } else {
        printf("%sOK%s CPU, memory, and root storage all have comfortable headroom.",
               ansi(ANSI_GREEN), ansi(ANSI_RESET));
    }
}

static int collect_overview(struct overview *view)
{
    struct cpu_sample cpu_prev;
    struct cpu_sample cpu_curr;
    struct network_sample net_prev;
    struct network_sample net_curr;
    struct timespec net_start;
    struct timespec net_end;

    memset(view, 0, sizeof(*view));
    view->network_ok = 1;

    if (memory_read(&view->memory) != 0 || disk_read_root(&view->disk) != 0 ||
        cpu_read(&cpu_prev) != 0) {
        return -1;
    }

    if (network_read(&net_prev) != 0 || clock_gettime(CLOCK_MONOTONIC, &net_start) != 0) {
        view->network_ok = 0;
    }

    if (wait_ns(SAMPLE_NS) != 0 || cpu_read(&cpu_curr) != 0) {
        return -1;
    }

    if (view->network_ok &&
        (network_read(&net_curr) != 0 || clock_gettime(CLOCK_MONOTONIC, &net_end) != 0 ||
         network_rate(&net_prev,
                      &net_curr,
                      elapsed_seconds(&net_start, &net_end),
                      &view->rx_per_sec,
                      &view->tx_per_sec) != 0)) {
        view->network_ok = 0;
    }

    if (cpu_usage(&cpu_prev, &cpu_curr, &view->cpu_percent) != 0 ||
        memory_usage(&view->memory,
                     &view->memory_percent,
                     &view->memory_used_kib,
                     &view->swap_used_kib) != 0 ||
        disk_usage(&view->disk, &view->disk_percent, &view->disk_used_bytes) != 0) {
        return -1;
    }

    view->process_ok = process_top_memory(view->process_top,
                                          TOP_PROCESS_COUNT,
                                          &view->process_count,
                                          &view->process_total) == 0;
    return 0;
}

static void render_compact(const struct overview *view, int live)
{
    printf("%sRookieTop%s  %s%s%s  %s%s%s\n",
           ansi(ANSI_BOLD),
           ansi(ANSI_RESET),
           ansi(ANSI_DIM),
           ROOKIETOP_VERSION,
           ansi(ANSI_RESET),
           ansi(overall_color(view->cpu_percent, view->memory_percent, view->disk_percent)),
           overall_status(view->cpu_percent, view->memory_percent, view->disk_percent),
           ansi(ANSI_RESET));
    print_rule_width(60);
    putchar('\n');

    print_section("SYSTEM");
    putchar('\n');
    print_metric("CPU", view->cpu_percent, cpu_status(view->cpu_percent),
                 cpu_color(view->cpu_percent), COMPACT_BAR_WIDTH);
    putchar('\n');
    print_metric("Memory", view->memory_percent, memory_status(view->memory_percent),
                 memory_color(view->memory_percent), COMPACT_BAR_WIDTH);
    putchar('\n');
    print_metric("Disk", view->disk_percent, disk_status(view->disk_percent),
                 disk_color(view->disk_percent), COMPACT_BAR_WIDTH);
    puts("\n");

    printf("RAM      %.1f / %.1f GiB used    %.1f GiB available\n",
           (double)view->memory_used_kib / KIB_PER_GIB,
           (double)view->memory.total_kib / KIB_PER_GIB,
           (double)view->memory.available_kib / KIB_PER_GIB);
    printf("Swap     %.1f / %.1f GiB used\n",
           (double)view->swap_used_kib / KIB_PER_GIB,
           (double)view->memory.swap_total_kib / KIB_PER_GIB);
    printf("Root     %.1f / %.1f GiB used    %.1f GiB available\n",
           (double)view->disk_used_bytes / BYTES_PER_GIB,
           (double)view->disk.total_bytes / BYTES_PER_GIB,
           (double)view->disk.available_bytes / BYTES_PER_GIB);

    if (view->network_ok) {
        printf("Network  down %.2f MiB/s          up %.2f MiB/s\n",
               view->rx_per_sec / BYTES_PER_MIB,
               view->tx_per_sec / BYTES_PER_MIB);
    } else {
        puts("Network  unavailable for this sample");
    }

    puts("");
    if (view->process_ok) {
        printf("%sTOP MEMORY%s  %s%zu readable processes%s\n",
               ansi(ANSI_BOLD), ansi(ANSI_RESET), ansi(ANSI_DIM),
               view->process_total, ansi(ANSI_RESET));
        puts("NAME                      RSS MiB      PID");
        for (size_t i = 0; i < view->process_count; i++) {
            printf("%-24s %7.1f  %8d\n",
                   view->process_top[i].name,
                   (double)view->process_top[i].rss_kib / KIB_PER_MIB,
                   view->process_top[i].pid);
        }
    } else {
        puts("TOP MEMORY  unavailable");
    }

    puts("\nINSIGHT");
    print_insight(view);
    putchar('\n');
    if (view->swap_used_kib > 0) {
        printf("%sNote:%s swap is in use; that alone does not mean the system is unhealthy.\n",
               ansi(ANSI_DIM), ansi(ANSI_RESET));
    }
    printf("\n%sSources: /proc/stat | /proc/meminfo | /proc/net/dev | /proc/<pid>/status | statvfs()%s\n",
           ansi(ANSI_DIM), ansi(ANSI_RESET));
    if (live) {
        printf("%sLive ~1s  |  Ctrl+C quit%s\n", ansi(ANSI_DIM), ansi(ANSI_RESET));
    }
}

static void render_fullscreen(const struct overview *view, struct terminal_size term)
{
    int full_width = term.cols - 2;
    int left_col = 2;
    int left_width = term.cols * 3 / 5;
    int right_col = left_width + 3;
    int bar_width = left_width - 30;
    int insight_row = term.rows - 7;
    int footer_rule_row = term.rows - 3;
    const char *health = overall_status(view->cpu_percent,
                                        view->memory_percent,
                                        view->disk_percent);
    int health_col = term.cols - (int)strlen(health) - 1;

    if (bar_width < 12) {
        bar_width = 12;
    }

    fputs("\033[H\033[J", stdout);

    move_to(1, left_col);
    printf("%sRookieTop%s  %s%s%s",
           ansi(ANSI_BOLD), ansi(ANSI_RESET),
           ansi(ANSI_DIM), ROOKIETOP_VERSION, ansi(ANSI_RESET));
    if (health_col > 30) {
        move_to(1, health_col);
    } else {
        putchar(' ');
    }
    printf("%s%s%s",
           ansi(overall_color(view->cpu_percent, view->memory_percent, view->disk_percent)),
           health,
           ansi(ANSI_RESET));

    move_to(2, left_col);
    print_rule_width(full_width);

    move_to(4, left_col);
    print_section("SYSTEM");
    move_to(5, left_col);
    print_metric("CPU", view->cpu_percent, cpu_status(view->cpu_percent),
                 cpu_color(view->cpu_percent), bar_width);
    move_to(6, left_col);
    print_metric("Memory", view->memory_percent, memory_status(view->memory_percent),
                 memory_color(view->memory_percent), bar_width);
    move_to(7, left_col);
    print_metric("Disk", view->disk_percent, disk_status(view->disk_percent),
                 disk_color(view->disk_percent), bar_width);

    move_to(4, right_col);
    print_section("DETAILS");
    move_to(5, right_col);
    printf("RAM      %.1f / %.1f GiB   %.1f GiB avail",
           (double)view->memory_used_kib / KIB_PER_GIB,
           (double)view->memory.total_kib / KIB_PER_GIB,
           (double)view->memory.available_kib / KIB_PER_GIB);
    move_to(6, right_col);
    printf("Swap     %.1f / %.1f GiB used",
           (double)view->swap_used_kib / KIB_PER_GIB,
           (double)view->memory.swap_total_kib / KIB_PER_GIB);
    move_to(7, right_col);
    printf("Root     %.1f / %.1f GiB   %.1f GiB avail",
           (double)view->disk_used_bytes / BYTES_PER_GIB,
           (double)view->disk.total_bytes / BYTES_PER_GIB,
           (double)view->disk.available_bytes / BYTES_PER_GIB);
    move_to(8, right_col);
    if (view->network_ok) {
        printf("Network  down %.2f MiB/s   up %.2f MiB/s",
               view->rx_per_sec / BYTES_PER_MIB,
               view->tx_per_sec / BYTES_PER_MIB);
    } else {
        printf("Network  unavailable");
    }

    move_to(10, left_col);
    print_rule_width(full_width);
    move_to(12, left_col);
    if (view->process_ok) {
        printf("%sTOP MEMORY%s  %s%zu readable processes%s",
               ansi(ANSI_BOLD), ansi(ANSI_RESET), ansi(ANSI_DIM),
               view->process_total, ansi(ANSI_RESET));
        move_to(13, left_col);
        printf("%-28s %10s %10s", "NAME", "RSS MiB", "PID");
        for (size_t i = 0; i < view->process_count; i++) {
            move_to(14 + (int)i, left_col);
            printf("%-28.28s %10.1f %10d",
                   view->process_top[i].name,
                   (double)view->process_top[i].rss_kib / KIB_PER_MIB,
                   view->process_top[i].pid);
        }
    } else {
        printf("%sTOP MEMORY%s  unavailable", ansi(ANSI_BOLD), ansi(ANSI_RESET));
    }

    if (insight_row < 18) {
        insight_row = 18;
    }
    move_to(insight_row, left_col);
    print_section("INSIGHT");
    move_to(insight_row + 1, left_col);
    print_insight(view);
    if (view->swap_used_kib > 0) {
        move_to(insight_row + 2, left_col);
        printf("%sNote:%s swap is in use; that alone does not mean the system is unhealthy.",
               ansi(ANSI_DIM), ansi(ANSI_RESET));
    }

    move_to(footer_rule_row, left_col);
    print_rule_width(full_width);
    move_to(term.rows - 2, left_col);
    printf("%sSources: /proc | statvfs()   no root | no daemon | direct Linux interfaces%s",
           ansi(ANSI_DIM), ansi(ANSI_RESET));
    move_to(term.rows - 1, left_col);
    printf("%sLIVE ~1s   Ctrl+C quit%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
}

static int show_overview(int live)
{
    struct overview view;

    if (collect_overview(&view) != 0) {
        fputs("rookietop: could not read system data\n", stderr);
        return 1;
    }

    if (!live) {
        render_compact(&view, 0);
        return 0;
    }

    struct terminal_size term = terminal_size();
    if (term.cols < MIN_FULL_COLS || term.rows < MIN_FULL_ROWS) {
        fputs("\033[H\033[J", stdout);
        render_compact(&view, 1);
        printf("%sResize to at least %dx%d for full-screen layout.%s\n",
               ansi(ANSI_DIM), MIN_FULL_COLS, MIN_FULL_ROWS, ansi(ANSI_RESET));
    } else {
        render_fullscreen(&view, term);
    }

    return 0;
}

static int run_monitor(int force_once)
{
    int terminal = isatty(STDOUT_FILENO);
    int live = !force_once && terminal;

    use_color = terminal && getenv("NO_COLOR") == NULL;

    if (!live) {
        return show_overview(0);
    }

    if (signal(SIGINT, handle_stop) == SIG_ERR || signal(SIGTERM, handle_stop) == SIG_ERR) {
        fputs("rookietop: could not install signal handler\n", stderr);
        return 1;
    }

    if (atexit(restore_terminal) != 0) {
        fputs("rookietop: could not register terminal cleanup\n", stderr);
        return 1;
    }

    fputs("\033[?1049h\033[?25l\033[2J\033[H", stdout);
    terminal_active = 1;

    while (!stop_requested) {
        int result = show_overview(1);
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

#define _POSIX_C_SOURCE 200809L

#include "cpu.h"
#include "disk.h"
#include "host.h"
#include "memory.h"
#include "network.h"
#include "process.h"
#include "process_cpu.h"
#include "process_list.h"
#include "terminal_input.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define ROOKIETOP_VERSION "0.1.0-alpha.5"
#define SAMPLE_NS 250000000L
#define FRAME_WAIT_MS 750
#define PROCESS_WAIT_MS 900
#define KIB_PER_GIB 1048576.0
#define KIB_PER_MIB 1024.0
#define BYTES_PER_GIB 1073741824.0
#define BYTES_PER_MIB 1048576.0
#define TOP_PROCESS_COUNT 8
#define COMPACT_PROCESS_COUNT 3
#define COMPACT_BAR_WIDTH 18
#define MIN_FULL_COLS 100
#define MIN_FULL_ROWS 32
#define HISTORY_MAX 90

#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_DIM "\033[2m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED "\033[31m"
#define ANSI_REVERSE "\033[7m"

struct overview {
    struct host_info host;
    struct memory_sample memory;
    struct disk_sample disk;
    struct process_info memory_top[TOP_PROCESS_COUNT];
    struct process_cpu_info cpu_top[TOP_PROCESS_COUNT];
    double cpu_percent;
    double memory_percent;
    double disk_percent;
    double rx_per_sec;
    double tx_per_sec;
    uint64_t memory_used_kib;
    uint64_t swap_used_kib;
    uint64_t disk_used_bytes;
    size_t memory_process_count;
    size_t memory_process_total;
    size_t cpu_process_count;
    size_t cpu_process_total;
    int host_ok;
    int network_ok;
    int memory_process_ok;
    int cpu_process_ok;
};

struct terminal_size {
    int rows;
    int cols;
};

struct metric_history {
    double cpu[HISTORY_MAX];
    double memory[HISTORY_MAX];
    size_t count;
};

enum app_screen {
    SCREEN_OVERVIEW,
    SCREEN_PROCESSES,
    SCREEN_DETAIL,
    SCREEN_CONFIRM_TERM,
    SCREEN_CONFIRM_KILL,
};

enum process_sort {
    SORT_MEMORY,
    SORT_PID,
    SORT_NAME,
};

struct app_state {
    enum app_screen screen;
    enum process_sort sort;
    size_t selected;
    int selected_pid;
    uint64_t selected_starttime;
    struct process_detail selected_detail;
    char notice[192];
};

static volatile sig_atomic_t stop_requested = 0;
static int use_color = 0;
static int terminal_active = 0;
static struct metric_history history;
static struct terminal_input input_state;

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
    terminal_input_restore(&input_state);
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
    puts("Interactive keys:");
    puts("  p                Open process explorer");
    puts("  Up/Down          Select process");
    puts("  Enter            Inspect selected process");
    puts("  k                Ask process to stop with SIGTERM");
    puts("  K                Force-stop with SIGKILL after confirmation");
    puts("  Esc              Go back");
    puts("  q                Quit");
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

static const char *thermal_color(double temperature)
{
    if (temperature >= 95.0) {
        return ANSI_RED;
    }
    return temperature >= 80.0 ? ANSI_YELLOW : ANSI_GREEN;
}

static double load_ratio(const struct overview *view)
{
    if (!view->host_ok || view->host.cores < 1) {
        return 0.0;
    }
    return view->host.load1 / (double)view->host.cores;
}

static const char *load_status(const struct overview *view)
{
    double ratio = load_ratio(view);
    if (ratio >= 1.5) {
        return "High";
    }
    if (ratio >= 0.8) {
        return "Busy";
    }
    return "Light";
}

static const char *load_color(const struct overview *view)
{
    double ratio = load_ratio(view);
    if (ratio >= 1.5) {
        return ANSI_RED;
    }
    return ratio >= 0.8 ? ANSI_YELLOW : ANSI_GREEN;
}

static const char *overall_status(const struct overview *view)
{
    if (view->disk_percent >= 95.0) {
        return "CRITICAL";
    }
    if (view->cpu_percent >= 90.0 || view->memory_percent >= 90.0 ||
        view->disk_percent >= 85.0 || load_ratio(view) >= 1.25 ||
        (view->host_ok && view->host.thermal_ok && view->host.thermal_c >= 90.0)) {
        return "ATTENTION";
    }
    return "HEALTHY";
}

static const char *overall_color(const struct overview *view)
{
    const char *status = overall_status(view);
    if (strcmp(status, "CRITICAL") == 0) {
        return ANSI_RED;
    }
    if (strcmp(status, "ATTENTION") == 0) {
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
    printf(" %5.1f%%  %s%s%s", percent, ansi(color), status, ansi(ANSI_RESET));
}

static void format_uptime(double seconds, char *out, size_t cap)
{
    long total_minutes = (long)(seconds / 60.0);
    long days = total_minutes / 1440;
    long hours = (total_minutes / 60) % 24;
    long minutes = total_minutes % 60;

    if (days > 0) {
        (void)snprintf(out, cap, "%ldd %02ldh", days, hours);
    } else if (hours > 0) {
        (void)snprintf(out, cap, "%ldh %02ldm", hours, minutes);
    } else {
        (void)snprintf(out, cap, "%ldm", minutes);
    }
}

static void history_push(const struct overview *view)
{
    if (history.count < HISTORY_MAX) {
        history.cpu[history.count] = view->cpu_percent;
        history.memory[history.count] = view->memory_percent;
        history.count++;
        return;
    }

    memmove(history.cpu, history.cpu + 1, (HISTORY_MAX - 1) * sizeof(history.cpu[0]));
    memmove(history.memory,
            history.memory + 1,
            (HISTORY_MAX - 1) * sizeof(history.memory[0]));
    history.cpu[HISTORY_MAX - 1] = view->cpu_percent;
    history.memory[HISTORY_MAX - 1] = view->memory_percent;
}

static void print_history_line(const char *label,
                               const double *values,
                               size_t count,
                               int graph_width,
                               const char *color)
{
    static const char levels[] = " .:-=+*#%@";
    size_t level_count = sizeof(levels) - 2;
    size_t shown = count < (size_t)graph_width ? count : (size_t)graph_width;
    size_t start = count - shown;
    int padding = graph_width - (int)shown;

    printf("%-7s ", label);
    fputs(ansi(ANSI_DIM), stdout);
    for (int i = 0; i < padding; i++) {
        putchar('.');
    }
    fputs(ansi(ANSI_RESET), stdout);
    fputs(ansi(color), stdout);
    for (size_t i = start; i < count; i++) {
        double value = values[i];
        if (value < 0.0) {
            value = 0.0;
        }
        if (value > 100.0) {
            value = 100.0;
        }
        size_t level = (size_t)(value * (double)level_count / 100.0);
        putchar(levels[level]);
    }
    fputs(ansi(ANSI_RESET), stdout);
}

static void print_insight(const struct overview *view)
{
    if (view->disk_percent >= 95.0) {
        printf("%s!%s Root storage is almost full. Free space soon.", ansi(ANSI_RED), ansi(ANSI_RESET));
    } else if (view->memory_percent >= 90.0) {
        printf("%s!%s Available memory is low. Sustained pressure can slow the system.", ansi(ANSI_YELLOW), ansi(ANSI_RESET));
    } else if (view->cpu_percent >= 90.0) {
        printf("%s!%s CPU is working very hard right now.", ansi(ANSI_YELLOW), ansi(ANSI_RESET));
    } else if (load_ratio(view) >= 1.25) {
        printf("%s!%s One-minute load is high for %ld online CPUs; work may be queueing or waiting on I/O.",
               ansi(ANSI_YELLOW), ansi(ANSI_RESET), view->host.cores);
    } else if (view->host_ok && view->host.thermal_ok && view->host.thermal_c >= 90.0) {
        printf("%s!%s The hottest exposed thermal zone is %.1f C.", ansi(ANSI_YELLOW), ansi(ANSI_RESET), view->host.thermal_c);
    } else if (view->disk_percent >= 85.0) {
        printf("%s!%s Root storage is getting full; keep an eye on free space.", ansi(ANSI_YELLOW), ansi(ANSI_RESET));
    } else {
        printf("%sOK%s CPU, memory, load, and root storage have comfortable headroom.", ansi(ANSI_GREEN), ansi(ANSI_RESET));
    }
}

static void print_load_guide(const struct overview *view)
{
    if (!view->host_ok) {
        return;
    }
    printf("%sLoad:%s %.2f over %ld CPUs is %s; load is queue pressure, not CPU percentage.",
           ansi(ANSI_DIM), ansi(ANSI_RESET), view->host.load1, view->host.cores, load_status(view));
}

static int collect_overview(struct overview *view)
{
    struct cpu_sample cpu_prev;
    struct cpu_sample cpu_curr;
    struct network_sample net_prev;
    struct network_sample net_curr;
    struct timespec net_start;
    struct timespec net_end;
    struct process_cpu_snapshot *process_prev = NULL;
    uint64_t system_delta_ticks;

    memset(view, 0, sizeof(*view));
    view->network_ok = 1;
    view->host_ok = host_read(&view->host) == 0;

    if (memory_read(&view->memory) != 0 || disk_read_root(&view->disk) != 0 || cpu_read(&cpu_prev) != 0) {
        return -1;
    }

    if (network_read(&net_prev) != 0 || clock_gettime(CLOCK_MONOTONIC, &net_start) != 0) {
        view->network_ok = 0;
    }

    process_prev = process_cpu_snapshot_take();
    if (wait_ns(SAMPLE_NS) != 0 || cpu_read(&cpu_curr) != 0) {
        process_cpu_snapshot_free(process_prev);
        return -1;
    }

    if (view->network_ok &&
        (network_read(&net_curr) != 0 || clock_gettime(CLOCK_MONOTONIC, &net_end) != 0 ||
         network_rate(&net_prev, &net_curr, elapsed_seconds(&net_start, &net_end),
                      &view->rx_per_sec, &view->tx_per_sec) != 0)) {
        view->network_ok = 0;
    }

    if (cpu_usage(&cpu_prev, &cpu_curr, &view->cpu_percent) != 0 ||
        cpu_total_delta(&cpu_prev, &cpu_curr, &system_delta_ticks) != 0 ||
        memory_usage(&view->memory, &view->memory_percent, &view->memory_used_kib,
                     &view->swap_used_kib) != 0 ||
        disk_usage(&view->disk, &view->disk_percent, &view->disk_used_bytes) != 0) {
        process_cpu_snapshot_free(process_prev);
        return -1;
    }

    if (process_prev != NULL) {
        view->cpu_process_ok = process_cpu_top_since(process_prev,
                                                      system_delta_ticks,
                                                      view->cpu_top,
                                                      TOP_PROCESS_COUNT,
                                                      &view->cpu_process_count,
                                                      &view->cpu_process_total) == 0;
    }
    process_cpu_snapshot_free(process_prev);

    view->memory_process_ok = process_top_memory(view->memory_top,
                                                 TOP_PROCESS_COUNT,
                                                 &view->memory_process_count,
                                                 &view->memory_process_total) == 0;
    return 0;
}

static void print_host_compact(const struct overview *view)
{
    if (!view->host_ok) {
        return;
    }

    char uptime[32];
    format_uptime(view->host.uptime_seconds, uptime, sizeof(uptime));
    printf("Host     %s | Linux %s | %ld CPU | up %s\n",
           view->host.hostname, view->host.kernel_release, view->host.cores, uptime);
    printf("Load     %.2f  %.2f  %.2f  %s%s%s\n",
           view->host.load1, view->host.load5, view->host.load15,
           ansi(load_color(view)), load_status(view), ansi(ANSI_RESET));
    if (view->host.thermal_ok) {
        printf("Thermal  %.1f C  %s%s%s  (%s)\n",
               view->host.thermal_c,
               ansi(thermal_color(view->host.thermal_c)),
               view->host.thermal_c >= 80.0 ? "Warm" : "Normal",
               ansi(ANSI_RESET), view->host.thermal_type);
    }
}

static void render_compact(const struct overview *view, int live)
{
    printf("%sRookieTop%s  %s%s%s  %s%s%s\n",
           ansi(ANSI_BOLD), ansi(ANSI_RESET), ansi(ANSI_DIM), ROOKIETOP_VERSION,
           ansi(ANSI_RESET), ansi(overall_color(view)), overall_status(view), ansi(ANSI_RESET));
    print_rule_width(60);
    putchar('\n');

    print_host_compact(view);
    if (view->host_ok) {
        putchar('\n');
    }

    print_section("SYSTEM");
    putchar('\n');
    print_metric("CPU", view->cpu_percent, cpu_status(view->cpu_percent), cpu_color(view->cpu_percent), COMPACT_BAR_WIDTH);
    putchar('\n');
    print_metric("Memory", view->memory_percent, memory_status(view->memory_percent), memory_color(view->memory_percent), COMPACT_BAR_WIDTH);
    putchar('\n');
    print_metric("Disk", view->disk_percent, disk_status(view->disk_percent), disk_color(view->disk_percent), COMPACT_BAR_WIDTH);
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
               view->rx_per_sec / BYTES_PER_MIB, view->tx_per_sec / BYTES_PER_MIB);
    } else {
        puts("Network  unavailable for this sample");
    }

    puts("\nINSIGHT");
    print_insight(view);
    putchar('\n');
    if (view->host_ok) {
        print_load_guide(view);
        putchar('\n');
    }
    printf("\n%sSources: procfs | sysfs | statvfs() | no root | no daemon%s\n", ansi(ANSI_DIM), ansi(ANSI_RESET));
    if (live) {
        printf("%sp processes  |  q quit  |  Ctrl+C quit%s\n", ansi(ANSI_DIM), ansi(ANSI_RESET));
    }
}

static void render_process_panel_cpu(const struct overview *view, int row, int col, int panel_width, int rows)
{
    move_to(row, col);
    printf("%sTOP CPU%s  %s250ms share of whole machine%s",
           ansi(ANSI_BOLD), ansi(ANSI_RESET), ansi(ANSI_DIM), ansi(ANSI_RESET));
    move_to(row + 1, col);

    int name_width = panel_width - 20;
    if (name_width > 32) {
        name_width = 32;
    }
    if (name_width < 12) {
        name_width = 12;
    }
    printf("%-*s %7s %8s", name_width, "NAME", "CPU %", "PID");

    if (!view->cpu_process_ok) {
        move_to(row + 2, col);
        printf("%sunavailable%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
        return;
    }

    int shown = (int)view->cpu_process_count < rows ? (int)view->cpu_process_count : rows;
    for (int i = 0; i < shown; i++) {
        move_to(row + 2 + i, col);
        printf("%-*.*s %7.2f %8d", name_width, name_width, view->cpu_top[i].name,
               view->cpu_top[i].cpu_percent, view->cpu_top[i].pid);
    }
    if (shown == 0) {
        move_to(row + 2, col);
        printf("%sno measurable CPU in this sample%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
    }
}

static void render_process_panel_memory(const struct overview *view, int row, int col, int panel_width, int rows)
{
    move_to(row, col);
    printf("%sTOP MEMORY%s  %s%zu readable processes%s",
           ansi(ANSI_BOLD), ansi(ANSI_RESET), ansi(ANSI_DIM), view->memory_process_total, ansi(ANSI_RESET));
    move_to(row + 1, col);

    int name_width = panel_width - 20;
    if (name_width > 32) {
        name_width = 32;
    }
    if (name_width < 12) {
        name_width = 12;
    }
    printf("%-*s %7s %8s", name_width, "NAME", "RSS MiB", "PID");

    if (!view->memory_process_ok) {
        move_to(row + 2, col);
        printf("%sunavailable%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
        return;
    }

    int shown = (int)view->memory_process_count < rows ? (int)view->memory_process_count : rows;
    for (int i = 0; i < shown; i++) {
        move_to(row + 2 + i, col);
        printf("%-*.*s %7.1f %8d", name_width, name_width, view->memory_top[i].name,
               (double)view->memory_top[i].rss_kib / KIB_PER_MIB, view->memory_top[i].pid);
    }
}

static void render_fullscreen(const struct overview *view, struct terminal_size term)
{
    int full_width = term.cols - 2;
    int left_col = 2;
    int left_width = term.cols * 3 / 5;
    int right_col = left_width + 3;
    int bar_width = left_width - 30;
    int panel_width = (full_width - 3) / 2;
    int process_right_col = left_col + panel_width + 3;
    int process_rows = term.rows - 27;
    int activity_row;
    int insight_row = term.rows - 7;
    int footer_rule_row = term.rows - 3;
    int graph_width = term.cols - 18;
    const char *health = overall_status(view);
    int health_col = term.cols - (int)strlen(health) - 1;

    if (bar_width < 12) {
        bar_width = 12;
    }
    if (process_rows < 3) {
        process_rows = 3;
    }
    if (process_rows > TOP_PROCESS_COUNT) {
        process_rows = TOP_PROCESS_COUNT;
    }
    if (graph_width > HISTORY_MAX) {
        graph_width = HISTORY_MAX;
    }
    if (graph_width < 20) {
        graph_width = 20;
    }
    activity_row = 16 + process_rows;

    fputs("\033[H\033[J", stdout);
    move_to(1, left_col);
    printf("%sRookieTop%s  %s%s%s", ansi(ANSI_BOLD), ansi(ANSI_RESET), ansi(ANSI_DIM), ROOKIETOP_VERSION, ansi(ANSI_RESET));
    if (health_col > 30) {
        move_to(1, health_col);
    } else {
        putchar(' ');
    }
    printf("%s%s%s", ansi(overall_color(view)), health, ansi(ANSI_RESET));

    move_to(2, left_col);
    if (view->host_ok) {
        char uptime[32];
        format_uptime(view->host.uptime_seconds, uptime, sizeof(uptime));
        printf("%s%s | Linux %s | %ld CPU | up %s | load %.2f %.2f %.2f%s",
               ansi(ANSI_DIM), view->host.hostname, view->host.kernel_release, view->host.cores,
               uptime, view->host.load1, view->host.load5, view->host.load15, ansi(ANSI_RESET));
    }

    move_to(3, left_col);
    print_rule_width(full_width);
    move_to(5, left_col);
    print_section("SYSTEM");
    move_to(6, left_col);
    print_metric("CPU", view->cpu_percent, cpu_status(view->cpu_percent), cpu_color(view->cpu_percent), bar_width);
    move_to(7, left_col);
    print_metric("Memory", view->memory_percent, memory_status(view->memory_percent), memory_color(view->memory_percent), bar_width);
    move_to(8, left_col);
    print_metric("Disk", view->disk_percent, disk_status(view->disk_percent), disk_color(view->disk_percent), bar_width);

    move_to(5, right_col);
    print_section("DETAILS");
    move_to(6, right_col);
    printf("RAM      %.1f / %.1f GiB   %.1f GiB avail",
           (double)view->memory_used_kib / KIB_PER_GIB,
           (double)view->memory.total_kib / KIB_PER_GIB,
           (double)view->memory.available_kib / KIB_PER_GIB);
    move_to(7, right_col);
    printf("Swap     %.1f / %.1f GiB used", (double)view->swap_used_kib / KIB_PER_GIB,
           (double)view->memory.swap_total_kib / KIB_PER_GIB);
    move_to(8, right_col);
    printf("Root     %.1f / %.1f GiB   %.1f GiB avail",
           (double)view->disk_used_bytes / BYTES_PER_GIB,
           (double)view->disk.total_bytes / BYTES_PER_GIB,
           (double)view->disk.available_bytes / BYTES_PER_GIB);
    move_to(9, right_col);
    if (view->network_ok) {
        printf("Network  down %.2f MiB/s   up %.2f MiB/s", view->rx_per_sec / BYTES_PER_MIB,
               view->tx_per_sec / BYTES_PER_MIB);
    } else {
        printf("Network  unavailable");
    }
    move_to(10, right_col);
    if (view->host_ok) {
        printf("Load     %.2f / %ld CPU   %s%s%s", view->host.load1, view->host.cores,
               ansi(load_color(view)), load_status(view), ansi(ANSI_RESET));
    } else {
        printf("Load     unavailable");
    }
    move_to(11, right_col);
    if (view->host_ok && view->host.thermal_ok) {
        printf("Thermal  %s%.1f C%s   %s", ansi(thermal_color(view->host.thermal_c)),
               view->host.thermal_c, ansi(ANSI_RESET), view->host.thermal_type);
    } else {
        printf("%sThermal  n/a (sensor not exposed)%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
    }

    move_to(12, left_col);
    print_rule_width(full_width);
    render_process_panel_cpu(view, 14, left_col, panel_width, process_rows);
    render_process_panel_memory(view, 14, process_right_col, panel_width, process_rows);

    move_to(activity_row, left_col);
    print_section("ACTIVITY");
    printf("  %slast ~%zu seconds%s", ansi(ANSI_DIM), history.count, ansi(ANSI_RESET));
    move_to(activity_row + 1, left_col);
    print_history_line("CPU", history.cpu, history.count, graph_width, cpu_color(view->cpu_percent));
    move_to(activity_row + 2, left_col);
    print_history_line("Memory", history.memory, history.count, graph_width, memory_color(view->memory_percent));

    move_to(insight_row, left_col);
    print_section("INSIGHT");
    move_to(insight_row + 1, left_col);
    print_insight(view);
    if (view->host_ok) {
        move_to(insight_row + 2, left_col);
        print_load_guide(view);
    }

    move_to(footer_rule_row, left_col);
    print_rule_width(full_width);
    move_to(term.rows - 2, left_col);
    printf("%sSources: procfs | sysfs | statvfs()   p = process explorer%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
    move_to(term.rows - 1, left_col);
    printf("%sp processes   q quit   LIVE ~1s   no root | no daemon%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
}

static int show_overview(int live)
{
    struct overview view;
    if (collect_overview(&view) != 0) {
        fputs("rookietop: could not read system data\n", stderr);
        return 1;
    }
    history_push(&view);

    if (!live) {
        render_compact(&view, 0);
        return 0;
    }

    struct terminal_size term = terminal_size();
    if (term.cols < MIN_FULL_COLS || term.rows < MIN_FULL_ROWS) {
        fputs("\033[H\033[J", stdout);
        render_compact(&view, 1);
        printf("%sResize to at least %dx%d for full dashboard.%s\n",
               ansi(ANSI_DIM), MIN_FULL_COLS, MIN_FULL_ROWS, ansi(ANSI_RESET));
    } else {
        render_fullscreen(&view, term);
    }
    return 0;
}

static int compare_memory(const void *a, const void *b)
{
    const struct process_row *left = a;
    const struct process_row *right = b;
    if (left->rss_kib < right->rss_kib) {
        return 1;
    }
    if (left->rss_kib > right->rss_kib) {
        return -1;
    }
    return (left->pid > right->pid) - (left->pid < right->pid);
}

static int compare_pid(const void *a, const void *b)
{
    const struct process_row *left = a;
    const struct process_row *right = b;
    return (left->pid > right->pid) - (left->pid < right->pid);
}

static int compare_name(const void *a, const void *b)
{
    const struct process_row *left = a;
    const struct process_row *right = b;
    int result = strcmp(left->name, right->name);
    return result != 0 ? result : compare_pid(a, b);
}

static const char *sort_name(enum process_sort sort)
{
    if (sort == SORT_PID) {
        return "PID";
    }
    if (sort == SORT_NAME) {
        return "name";
    }
    return "memory";
}

static void sort_processes(struct process_row *rows, size_t count, enum process_sort sort)
{
    if (count < 2) {
        return;
    }
    if (sort == SORT_PID) {
        qsort(rows, count, sizeof(rows[0]), compare_pid);
    } else if (sort == SORT_NAME) {
        qsort(rows, count, sizeof(rows[0]), compare_name);
    } else {
        qsort(rows, count, sizeof(rows[0]), compare_memory);
    }
}

static void align_selection(struct app_state *state, const struct process_row *rows, size_t count)
{
    if (count == 0) {
        state->selected = 0;
        state->selected_pid = 0;
        state->selected_starttime = 0;
        return;
    }

    if (state->selected_pid > 0) {
        for (size_t i = 0; i < count; i++) {
            if (rows[i].pid == state->selected_pid && rows[i].starttime == state->selected_starttime) {
                state->selected = i;
                return;
            }
        }
    }

    if (state->selected >= count) {
        state->selected = count - 1;
    }
    state->selected_pid = rows[state->selected].pid;
    state->selected_starttime = rows[state->selected].starttime;
}

static const char *state_meaning(char state)
{
    if (state == 'R') {
        return "running";
    }
    if (state == 'S') {
        return "sleeping";
    }
    if (state == 'D') {
        return "waiting on I/O";
    }
    if (state == 'T' || state == 't') {
        return "stopped";
    }
    if (state == 'Z') {
        return "zombie";
    }
    return "other";
}

static void render_processes(const struct process_row *rows,
                             size_t count,
                             const struct app_state *state,
                             struct terminal_size term)
{
    int left = 2;
    int width = term.cols - 2;
    int table_row = 5;
    int footer_row = term.rows - 2;
    int visible = footer_row - table_row - 2;
    int name_width = term.cols - 43;
    if (visible < 3) {
        visible = 3;
    }
    if (name_width < 12) {
        name_width = 12;
    }
    if (name_width > 70) {
        name_width = 70;
    }

    size_t start = 0;
    if (count > 0 && state->selected >= (size_t)visible) {
        start = state->selected - (size_t)visible + 1;
    }

    fputs("\033[H\033[J", stdout);
    move_to(1, left);
    printf("%sRookieTop%s  %s%s%s  PROCESS EXPLORER",
           ansi(ANSI_BOLD), ansi(ANSI_RESET), ansi(ANSI_DIM), ROOKIETOP_VERSION, ansi(ANSI_RESET));
    move_to(2, left);
    printf("%s%zu readable processes | sorted by %s | signals are never escalated automatically%s",
           ansi(ANSI_DIM), count, sort_name(state->sort), ansi(ANSI_RESET));
    move_to(3, left);
    print_rule_width(width);
    move_to(table_row, left);
    printf("  %-8s %9s %5s %7s %-*s", "PID", "RSS MiB", "STATE", "THREADS", name_width, "NAME");

    int row = table_row + 1;
    for (size_t i = start; i < count && row < footer_row - 1; i++, row++) {
        move_to(row, left);
        if (i == state->selected) {
            fputs(ansi(ANSI_REVERSE), stdout);
            printf("> %-8d %9.1f %5c %7lu %-*.*s",
                   rows[i].pid,
                   (double)rows[i].rss_kib / KIB_PER_MIB,
                   rows[i].state,
                   rows[i].threads,
                   name_width,
                   name_width,
                   rows[i].name);
            fputs(ansi(ANSI_RESET), stdout);
        } else {
            printf("  %-8d %9.1f %5c %7lu %-*.*s",
                   rows[i].pid,
                   (double)rows[i].rss_kib / KIB_PER_MIB,
                   rows[i].state,
                   rows[i].threads,
                   name_width,
                   name_width,
                   rows[i].name);
        }
    }

    if (count == 0) {
        move_to(table_row + 2, left);
        printf("%sNo readable processes.%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
    }

    if (state->notice[0] != '\0') {
        move_to(footer_row - 1, left);
        printf("%s%s%s", ansi(ANSI_YELLOW), state->notice, ansi(ANSI_RESET));
    }
    move_to(footer_row, left);
    print_rule_width(width);
    move_to(footer_row + 1, left);
    printf("%sUp/Down select | Enter inspect | m memory | p PID | n name | k stop | K force | Esc overview | q quit%s",
           ansi(ANSI_DIM), ansi(ANSI_RESET));
}

static void render_detail(const struct process_detail *detail, struct terminal_size term)
{
    int left = 3;
    int width = term.cols - 5;
    fputs("\033[H\033[J", stdout);
    move_to(1, left);
    printf("%sPROCESS DETAIL%s", ansi(ANSI_BOLD), ansi(ANSI_RESET));
    move_to(2, left);
    print_rule_width(width);
    move_to(4, left);
    printf("Name       %s", detail->row.name);
    move_to(5, left);
    printf("PID        %d", detail->row.pid);
    move_to(6, left);
    printf("State      %c  (%s)", detail->row.state, state_meaning(detail->row.state));
    move_to(7, left);
    printf("Threads    %lu", detail->row.threads);
    move_to(8, left);
    printf("Memory     %.1f MiB RSS", (double)detail->row.rss_kib / KIB_PER_MIB);
    move_to(10, left);
    print_section("COMMAND");
    move_to(11, left);
    printf("%.*s", width, detail->command);
    move_to(13, left);
    printf("%sRookieTop tracks the process start time before signalling it, so PID reuse cannot target a different process.%s",
           ansi(ANSI_DIM), ansi(ANSI_RESET));
    move_to(term.rows - 2, left);
    print_rule_width(width);
    move_to(term.rows - 1, left);
    printf("%sk graceful stop | K force stop | Esc back | q quit%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
}

static void render_confirm(const struct process_detail *detail,
                           struct terminal_size term,
                           int force)
{
    int left = 3;
    int width = term.cols - 5;
    fputs("\033[H\033[J", stdout);
    move_to(1, left);
    if (force) {
        printf("%sFORCE STOP PROCESS?%s", ansi(ANSI_RED), ansi(ANSI_RESET));
    } else {
        printf("%sSTOP PROCESS?%s", ansi(ANSI_YELLOW), ansi(ANSI_RESET));
    }
    move_to(2, left);
    print_rule_width(width);
    move_to(4, left);
    printf("%s  PID %d", detail->row.name, detail->row.pid);
    move_to(6, left);
    if (force) {
        puts("SIGKILL asks the kernel to stop this process immediately.");
        move_to(7, left);
        puts("The process cannot catch SIGKILL or run cleanup handlers.");
        move_to(9, left);
        printf("%sUse this only when a graceful SIGTERM did not work.%s", ansi(ANSI_RED), ansi(ANSI_RESET));
    } else {
        puts("SIGTERM asks the process to shut down cleanly.");
        move_to(7, left);
        puts("The process may save state, close files, and exit on its own.");
        move_to(9, left);
        printf("%sRookieTop will not escalate to SIGKILL automatically.%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
    }
    move_to(term.rows - 2, left);
    print_rule_width(width);
    move_to(term.rows - 1, left);
    printf("%sy confirm | n/Esc cancel | q quit%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
}

static void set_notice(struct app_state *state, const char *message)
{
    (void)snprintf(state->notice, sizeof(state->notice), "%s", message);
}

static void set_signal_notice(struct app_state *state,
                              enum process_signal_result result,
                              int force,
                              const char *name)
{
    if (result == PROCESS_SIGNAL_OK) {
        if (force) {
            (void)snprintf(state->notice, sizeof(state->notice),
                           "SIGKILL sent to %s. The kernel will stop it immediately.", name);
        } else {
            (void)snprintf(state->notice, sizeof(state->notice),
                           "SIGTERM sent to %s. If it refuses to exit, select it and press K to force.", name);
        }
    } else if (result == PROCESS_SIGNAL_PERMISSION) {
        set_notice(state, "Permission denied. The process may belong to another user or be protected by system policy.");
    } else if (result == PROCESS_SIGNAL_REUSED) {
        set_notice(state, "PID was reused by another process. No signal was sent.");
    } else if (result == PROCESS_SIGNAL_GONE) {
        set_notice(state, "The process already exited. No signal was sent.");
    } else {
        set_notice(state, "Could not signal the process.");
    }
}

static void open_selected_detail(struct app_state *state, const struct process_row *row)
{
    state->selected_pid = row->pid;
    state->selected_starttime = row->starttime;
    if (process_detail_read(row->pid, &state->selected_detail) == 0 &&
        state->selected_detail.row.starttime == row->starttime) {
        state->screen = SCREEN_DETAIL;
    } else {
        set_notice(state, "Process changed or exited before it could be inspected.");
        state->screen = SCREEN_PROCESSES;
    }
}

static void open_signal_confirm(struct app_state *state,
                                const struct process_row *row,
                                int force)
{
    state->selected_pid = row->pid;
    state->selected_starttime = row->starttime;
    if (process_detail_read(row->pid, &state->selected_detail) != 0 ||
        state->selected_detail.row.starttime != row->starttime) {
        set_notice(state, "Process changed or exited before it could be selected.");
        state->screen = SCREEN_PROCESSES;
        return;
    }
    state->screen = force ? SCREEN_CONFIRM_KILL : SCREEN_CONFIRM_TERM;
}

static void handle_signal_confirm(struct app_state *state, int force)
{
    if (state->selected_detail.row.pid == (int)getpid()) {
        set_notice(state, "RookieTop will not signal its own process.");
        state->screen = SCREEN_PROCESSES;
        return;
    }

    int signal_number = force ? SIGKILL : SIGTERM;
    enum process_signal_result result = process_send_signal(state->selected_detail.row.pid,
                                                            state->selected_detail.row.starttime,
                                                            signal_number);
    set_signal_notice(state, result, force, state->selected_detail.row.name);
    state->screen = SCREEN_PROCESSES;
}

static int run_interactive(void)
{
    struct app_state state;
    memset(&state, 0, sizeof(state));
    state.screen = SCREEN_OVERVIEW;
    state.sort = SORT_MEMORY;

    if (signal(SIGINT, handle_stop) == SIG_ERR || signal(SIGTERM, handle_stop) == SIG_ERR) {
        fputs("rookietop: could not install signal handler\n", stderr);
        return 1;
    }
    if (atexit(restore_terminal) != 0) {
        fputs("rookietop: could not register terminal cleanup\n", stderr);
        return 1;
    }
    if (terminal_input_enable(&input_state) != 0) {
        fputs("rookietop: could not enable terminal input\n", stderr);
        return 1;
    }

    fputs("\033[?1049h\033[?25l\033[2J\033[H", stdout);
    terminal_active = 1;

    while (!stop_requested) {
        int key = INPUT_NONE;
        struct terminal_size term = terminal_size();

        if (state.screen == SCREEN_OVERVIEW) {
            if (show_overview(1) != 0) {
                return 1;
            }
            fflush(stdout);
            key = terminal_input_read_key(FRAME_WAIT_MS);
            if (key == 'p') {
                state.screen = SCREEN_PROCESSES;
                state.notice[0] = '\0';
            }
        } else if (state.screen == SCREEN_PROCESSES) {
            struct process_row *rows = NULL;
            size_t count = 0;
            if (process_list_read(&rows, &count) != 0) {
                set_notice(&state, "Could not read process list.");
                count = 0;
            }
            sort_processes(rows, count, state.sort);
            align_selection(&state, rows, count);
            render_processes(rows, count, &state, term);
            fflush(stdout);
            key = terminal_input_read_key(PROCESS_WAIT_MS);

            if (key == INPUT_UP && count > 0) {
                if (state.selected > 0) {
                    state.selected--;
                }
                state.selected_pid = rows[state.selected].pid;
                state.selected_starttime = rows[state.selected].starttime;
            } else if (key == INPUT_DOWN && count > 0) {
                if (state.selected + 1 < count) {
                    state.selected++;
                }
                state.selected_pid = rows[state.selected].pid;
                state.selected_starttime = rows[state.selected].starttime;
            } else if (key == INPUT_ENTER && count > 0) {
                open_selected_detail(&state, &rows[state.selected]);
            } else if (key == 'k' && count > 0) {
                open_signal_confirm(&state, &rows[state.selected], 0);
            } else if (key == 'K' && count > 0) {
                open_signal_confirm(&state, &rows[state.selected], 1);
            } else if (key == 'm') {
                state.sort = SORT_MEMORY;
                state.selected_pid = 0;
                state.selected = 0;
            } else if (key == 'p') {
                state.sort = SORT_PID;
                state.selected_pid = 0;
                state.selected = 0;
            } else if (key == 'n') {
                state.sort = SORT_NAME;
                state.selected_pid = 0;
                state.selected = 0;
            } else if (key == INPUT_ESCAPE) {
                state.screen = SCREEN_OVERVIEW;
            }
            process_list_free(rows);
        } else if (state.screen == SCREEN_DETAIL) {
            struct process_detail detail;
            if (process_detail_read(state.selected_pid, &detail) != 0 ||
                detail.row.starttime != state.selected_starttime) {
                set_notice(&state, "Process exited or its PID was reused.");
                state.screen = SCREEN_PROCESSES;
                continue;
            }
            state.selected_detail = detail;
            render_detail(&detail, term);
            fflush(stdout);
            key = terminal_input_read_key(PROCESS_WAIT_MS);
            if (key == 'k') {
                state.screen = SCREEN_CONFIRM_TERM;
            } else if (key == 'K') {
                state.screen = SCREEN_CONFIRM_KILL;
            } else if (key == INPUT_ESCAPE) {
                state.screen = SCREEN_PROCESSES;
            }
        } else {
            int force = state.screen == SCREEN_CONFIRM_KILL;
            render_confirm(&state.selected_detail, term, force);
            fflush(stdout);
            key = terminal_input_read_key(-1);
            if (key == 'y' || key == 'Y') {
                handle_signal_confirm(&state, force);
            } else if (key == 'n' || key == 'N' || key == INPUT_ESCAPE) {
                state.screen = SCREEN_PROCESSES;
            }
        }

        if (key < 0) {
            return 1;
        }
        if (key == 'q' || key == 'Q') {
            break;
        }
    }

    return 0;
}

static int run_monitor(int force_once)
{
    int output_terminal = isatty(STDOUT_FILENO);
    int interactive = !force_once && output_terminal && isatty(STDIN_FILENO);
    use_color = output_terminal && getenv("NO_COLOR") == NULL;

    if (!interactive) {
        return show_overview(0);
    }
    return run_interactive();
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

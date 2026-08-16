#define _POSIX_C_SOURCE 200809L

#include "cpu.h"
#include "diagnosis.h"
#include "disk.h"
#include "host.h"
#include "memory.h"
#include "network.h"
#include "process.h"
#include "process_cpu.h"
#include "process_list.h"
#include "teaching.h"
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

#define ROOKIETOP_VERSION "0.1.0-alpha.7"
#define SAMPLE_NS 250000000L
#define FRAME_WAIT_MS 700
#define PROCESS_WAIT_MS 900
#define KIB_PER_GIB 1048576.0
#define KIB_PER_MIB 1024.0
#define BYTES_PER_GIB 1073741824.0
#define BYTES_PER_MIB 1048576.0
#define TOP_PROCESS_COUNT 8
#define HISTORY_MAX 90
#define MIN_FULL_COLS 100
#define MIN_FULL_ROWS 32

#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_DIM "\033[2m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED "\033[31m"
#define ANSI_CYAN "\033[36m"
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
    SCREEN_PROCESS_LEARN,
    SCREEN_LEARN_MENU,
    SCREEN_LEARN_TOPIC,
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
    enum app_screen return_screen;
    enum process_sort sort;
    enum teaching_concept lesson;
    size_t learn_selected;
    size_t selected;
    int selected_pid;
    uint64_t selected_starttime;
    struct process_detail selected_detail;
    struct overview view;
    int view_ok;
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
    puts("RookieTop - understand Linux by watching your own system");
    puts("");
    puts("Usage: rookietop [OPTION]");
    puts("");
    puts("Options:");
    puts("  --once           Print one snapshot and exit");
    puts("  -h, --help       Show this help");
    puts("  -V, --version    Show version");
    puts("");
    puts("Interactive keys:");
    puts("  ? or l           Explain what you are looking at");
    puts("  p                Open Process Explorer");
    puts("  Up/Down          Move through a list");
    puts("  Enter            Open the selected item");
    puts("  k                Ask a process to stop cleanly");
    puts("  K                Force-kill after a separate warning");
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

static void print_rule(int width)
{
    for (int i = 0; i < width; i++) {
        putchar('-');
    }
}

static void print_section(const char *name)
{
    printf("%s%s%s", ansi(ANSI_BOLD), name, ansi(ANSI_RESET));
}

static void print_key(const char *key, const char *label, const char *color)
{
    printf("%s%s[%s]%s %s", ansi(ANSI_BOLD), ansi(color), key, ansi(ANSI_RESET), label);
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

static const char *metric_color(double usage, double warn, double critical)
{
    if (usage >= critical) {
        return ANSI_RED;
    }
    if (usage >= warn) {
        return ANSI_YELLOW;
    }
    return ANSI_GREEN;
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

static void print_history(const char *label,
                          const double *values,
                          size_t count,
                          int width,
                          const char *color)
{
    static const char levels[] = " .:-=+*#%@";
    const size_t max_level = sizeof(levels) - 2;
    size_t shown = count < (size_t)width ? count : (size_t)width;
    size_t start = count - shown;
    int padding = width - (int)shown;

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
        size_t level = (size_t)(value * (double)max_level / 100.0);
        putchar(levels[level]);
    }
    fputs(ansi(ANSI_RESET), stdout);
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

    if (memory_read(&view->memory) != 0 || disk_read_root(&view->disk) != 0 ||
        cpu_read(&cpu_prev) != 0) {
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
         network_rate(&net_prev,
                      &net_curr,
                      elapsed_seconds(&net_start, &net_end),
                      &view->rx_per_sec,
                      &view->tx_per_sec) != 0)) {
        view->network_ok = 0;
    }

    if (cpu_usage(&cpu_prev, &cpu_curr, &view->cpu_percent) != 0 ||
        cpu_total_delta(&cpu_prev, &cpu_curr, &system_delta_ticks) != 0 ||
        memory_usage(&view->memory,
                     &view->memory_percent,
                     &view->memory_used_kib,
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

static int build_view_diagnosis(const struct overview *view, struct diagnosis *out)
{
    struct diagnosis_input input;
    memset(&input, 0, sizeof(input));
    input.cpu_percent = view->cpu_percent;
    input.memory_percent = view->memory_percent;
    input.disk_percent = view->disk_percent;
    input.load_ratio = load_ratio(view);
    input.memory_available_kib = view->memory.available_kib;
    input.disk_available_bytes = view->disk.available_bytes;

    if (view->cpu_process_ok && view->cpu_process_count > 0) {
        input.top_cpu_name = view->cpu_top[0].name;
        input.top_cpu_percent = view->cpu_top[0].cpu_percent;
    }
    if (view->memory_process_ok && view->memory_process_count > 0) {
        input.top_memory_name = view->memory_top[0].name;
        input.top_memory_kib = view->memory_top[0].rss_kib;
    }
    return diagnosis_build(&input, out);
}

static const char *diagnosis_color(const struct diagnosis *diagnosis)
{
    if (diagnosis->focus == DIAGNOSIS_HEALTHY) {
        return ANSI_GREEN;
    }
    if (diagnosis->focus == DIAGNOSIS_DISK) {
        return ANSI_RED;
    }
    return ANSI_YELLOW;
}

static enum teaching_concept diagnosis_lesson(const struct overview *view)
{
    struct diagnosis diagnosis;
    if (build_view_diagnosis(view, &diagnosis) != 0) {
        return TEACH_CPU;
    }
    switch (diagnosis.focus) {
    case DIAGNOSIS_CPU: return TEACH_CPU;
    case DIAGNOSIS_MEMORY: return TEACH_MEMORY;
    case DIAGNOSIS_DISK: return TEACH_DISK;
    case DIAGNOSIS_LOAD: return TEACH_LOAD;
    case DIAGNOSIS_HEALTHY:
    default: return TEACH_CPU;
    }
}

static void print_insight(const struct overview *view)
{
    struct diagnosis diagnosis;
    if (build_view_diagnosis(view, &diagnosis) != 0) {
        fputs("RookieTop could not build an explanation for this sample.", stdout);
        return;
    }
    printf("%s%s%s\n%s", ansi(diagnosis_color(&diagnosis)), diagnosis.headline,
           ansi(ANSI_RESET), diagnosis.detail);
}

static void render_compact(const struct overview *view)
{
    printf("RookieTop %s  %s\n", ROOKIETOP_VERSION, overall_status(view));
    print_rule(64);
    putchar('\n');
    print_metric("CPU", view->cpu_percent, cpu_status(view->cpu_percent), "", 18);
    putchar('\n');
    print_metric("Memory", view->memory_percent, memory_status(view->memory_percent), "", 18);
    putchar('\n');
    print_metric("Disk", view->disk_percent, disk_status(view->disk_percent), "", 18);
    putchar('\n');
    printf("RAM      %.1f / %.1f GiB used | %.1f GiB available\n",
           (double)view->memory_used_kib / KIB_PER_GIB,
           (double)view->memory.total_kib / KIB_PER_GIB,
           (double)view->memory.available_kib / KIB_PER_GIB);
    if (view->network_ok) {
        printf("Network  down %.2f MiB/s | up %.2f MiB/s\n",
               view->rx_per_sec / BYTES_PER_MIB,
               view->tx_per_sec / BYTES_PER_MIB);
    }
    puts("\nWhat RookieTop notices");
    print_insight(view);
    puts("\n\nRun RookieTop in a terminal and press ? when you want the deeper Linux explanation.");
}

static void render_overview(const struct overview *view, struct terminal_size term)
{
    int left = 2;
    int width = term.cols - 3;
    int split = term.cols * 3 / 5;
    int right = split + 2;
    int bar_width = split - 29;
    int process_rows = term.rows >= 40 ? 6 : 4;
    int process_row = 14;
    int activity_row = process_row + process_rows + 3;
    int teaching_row = term.rows - 8;
    int footer_row = term.rows - 2;
    int graph_width = width - 12;
    const char *health = overall_status(view);
    struct diagnosis diagnosis;

    (void)build_view_diagnosis(view, &diagnosis);

    if (bar_width < 14) {
        bar_width = 14;
    }
    if (graph_width > HISTORY_MAX) {
        graph_width = HISTORY_MAX;
    }
    if (graph_width < 20) {
        graph_width = 20;
    }

    fputs("\033[H\033[J", stdout);
    move_to(1, left);
    printf("%sRookieTop%s  %s%s%s  %sYOUR LINUX, EXPLAINED%s",
           ansi(ANSI_BOLD), ansi(ANSI_RESET),
           ansi(ANSI_DIM), ROOKIETOP_VERSION, ansi(ANSI_RESET),
           ansi(ANSI_CYAN), ansi(ANSI_RESET));
    move_to(1, term.cols - (int)strlen(health) - 1);
    printf("%s%s%s", ansi(overall_color(view)), health, ansi(ANSI_RESET));

    move_to(2, left);
    if (view->host_ok) {
        char uptime[32];
        format_uptime(view->host.uptime_seconds, uptime, sizeof(uptime));
        printf("%s%s | Linux %s | %ld CPU | up %s | load %.2f %.2f %.2f%s",
               ansi(ANSI_DIM), view->host.hostname, view->host.kernel_release, view->host.cores,
               uptime, view->host.load1, view->host.load5, view->host.load15, ansi(ANSI_RESET));
    }
    move_to(3, left);
    print_rule(width);

    move_to(5, left);
    print_section("SYSTEM");
    move_to(6, left);
    print_metric("CPU", view->cpu_percent, cpu_status(view->cpu_percent),
                 metric_color(view->cpu_percent, 70.0, 95.0), bar_width);
    move_to(7, left);
    print_metric("Memory", view->memory_percent, memory_status(view->memory_percent),
                 metric_color(view->memory_percent, 80.0, 95.0), bar_width);
    move_to(8, left);
    print_metric("Disk", view->disk_percent, disk_status(view->disk_percent),
                 metric_color(view->disk_percent, 85.0, 95.0), bar_width);

    move_to(5, right);
    print_section("RIGHT NOW");
    move_to(6, right);
    printf("RAM      %.1f / %.1f GiB   %.1f GiB available",
           (double)view->memory_used_kib / KIB_PER_GIB,
           (double)view->memory.total_kib / KIB_PER_GIB,
           (double)view->memory.available_kib / KIB_PER_GIB);
    move_to(7, right);
    printf("Root     %.1f / %.1f GiB   %.1f GiB available",
           (double)view->disk_used_bytes / BYTES_PER_GIB,
           (double)view->disk.total_bytes / BYTES_PER_GIB,
           (double)view->disk.available_bytes / BYTES_PER_GIB);
    move_to(8, right);
    if (view->network_ok) {
        printf("Network  down %.2f MiB/s   up %.2f MiB/s",
               view->rx_per_sec / BYTES_PER_MIB,
               view->tx_per_sec / BYTES_PER_MIB);
    } else {
        printf("Network  unavailable this sample");
    }
    move_to(9, right);
    if (view->host_ok) {
        printf("Load     %.2f / %ld CPU   %s", view->host.load1, view->host.cores, load_status(view));
    }
    move_to(10, right);
    if (view->host_ok && view->host.thermal_ok) {
        printf("Thermal  %.1f C   %s", view->host.thermal_c, view->host.thermal_type);
    } else {
        printf("%sThermal  not exposed by this system%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
    }

    move_to(12, left);
    print_rule(width);
    move_to(process_row, left);
    printf("%sBUSIEST CPU%s", ansi(ANSI_BOLD), ansi(ANSI_RESET));
    move_to(process_row, right);
    printf("%sBIGGEST MEMORY USERS%s", ansi(ANSI_BOLD), ansi(ANSI_RESET));

    for (int i = 0; i < process_rows; i++) {
        move_to(process_row + 1 + i, left);
        if (view->cpu_process_ok && (size_t)i < view->cpu_process_count) {
            printf("%-22.22s %6.2f%%  pid %-7d",
                   view->cpu_top[i].name, view->cpu_top[i].cpu_percent, view->cpu_top[i].pid);
        } else if (i == 0) {
            printf("%sNo process stood out in this short sample.%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
        }

        move_to(process_row + 1 + i, right);
        if (view->memory_process_ok && (size_t)i < view->memory_process_count) {
            printf("%-22.22s %7.1f MiB  pid %-7d",
                   view->memory_top[i].name,
                   (double)view->memory_top[i].rss_kib / KIB_PER_MIB,
                   view->memory_top[i].pid);
        }
    }

    if (activity_row + 2 < teaching_row) {
        move_to(activity_row, left);
        print_section("RECENT ACTIVITY");
        printf("  %s~%zu samples%s", ansi(ANSI_DIM), history.count, ansi(ANSI_RESET));
        move_to(activity_row + 1, left);
        print_history("CPU", history.cpu, history.count, graph_width,
                      metric_color(view->cpu_percent, 70.0, 95.0));
        move_to(activity_row + 2, left);
        print_history("Memory", history.memory, history.count, graph_width,
                      metric_color(view->memory_percent, 80.0, 95.0));
    }

    move_to(teaching_row, left);
    printf("%sWHAT ROOKIETOP NOTICES%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
    move_to(teaching_row + 1, left);
    printf("%s%s%s", ansi(diagnosis_color(&diagnosis)), diagnosis.headline, ansi(ANSI_RESET));
    move_to(teaching_row + 2, left);
    printf("%.*s", width, diagnosis.detail);
    move_to(teaching_row + 3, left);
    if (diagnosis.focus == DIAGNOSIS_HEALTHY) {
        printf("Curious about a number? Press %s?%s to see how Linux produces it and try it yourself.",
               ansi(ANSI_BOLD), ansi(ANSI_RESET));
    } else {
        printf("Want the deeper reason? Press %s?%s; RookieTop will start with the most relevant concept.",
               ansi(ANSI_BOLD), ansi(ANSI_RESET));
    }

    move_to(footer_row, left);
    print_rule(width);
    move_to(footer_row + 1, left);
    print_key("?", "Explain", ANSI_CYAN);
    printf("   ");
    print_key("p", "Processes", ANSI_GREEN);
    printf("   ");
    print_key("q", "Quit", ANSI_DIM);
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

static void render_processes(const struct process_row *rows,
                             size_t count,
                             const struct app_state *state,
                             struct terminal_size term)
{
    int left = 2;
    int width = term.cols - 3;
    int table_row = 7;
    int about_row = term.rows - 9;
    int footer_row = term.rows - 2;
    int visible = about_row - table_row - 1;
    int name_width = term.cols - 45;

    if (visible < 4) {
        visible = 4;
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
    printf("%sRookieTop%s  %sPROCESS EXPLORER%s",
           ansi(ANSI_BOLD), ansi(ANSI_RESET), ansi(ANSI_CYAN), ansi(ANSI_RESET));
    move_to(2, left);
    printf("%sPick a process to see what it is doing and why it matters.%s",
           ansi(ANSI_CYAN), ansi(ANSI_RESET));
    move_to(3, left);
    printf("%s%zu readable processes | sorted by %s%s",
           ansi(ANSI_DIM), count, sort_name(state->sort), ansi(ANSI_RESET));
    move_to(4, left);
    print_rule(width);

    move_to(table_row - 1, left);
    printf("  %-8s %11s %13s %7s %-*s", "PID", "MEMORY", "STATE", "THREADS", name_width, "NAME");

    int row = table_row;
    for (size_t i = start; i < count && row < about_row - 1; i++, row++) {
        move_to(row, left);
        const char *state_name = teaching_state_name(rows[i].state);
        if (i == state->selected) {
            fputs(ansi(ANSI_REVERSE), stdout);
            printf("> %-8d %8.1f MiB %13.13s %7lu %-*.*s",
                   rows[i].pid,
                   (double)rows[i].rss_kib / KIB_PER_MIB,
                   state_name,
                   rows[i].threads,
                   name_width,
                   name_width,
                   rows[i].name);
            fputs(ansi(ANSI_RESET), stdout);
        } else {
            printf("  %-8d %8.1f MiB %13.13s %7lu %-*.*s",
                   rows[i].pid,
                   (double)rows[i].rss_kib / KIB_PER_MIB,
                   state_name,
                   rows[i].threads,
                   name_width,
                   name_width,
                   rows[i].name);
        }
    }

    move_to(about_row, left);
    print_rule(width);
    move_to(about_row + 1, left);
    printf("%sABOUT THIS PROCESS%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
    if (count > 0) {
        const struct process_row *selected = &rows[state->selected];
        move_to(about_row + 2, left);
        printf("%s  PID %d  |  %s  |  %.1f MiB  |  %lu thread%s",
               selected->name,
               selected->pid,
               teaching_state_name(selected->state),
               (double)selected->rss_kib / KIB_PER_MIB,
               selected->threads,
               selected->threads == 1 ? "" : "s");
        move_to(about_row + 3, left);
        printf("%s", teaching_state_explanation(selected->state));
        move_to(about_row + 4, left);
        printf("Curious how Linux sees this process? Press %s?%s to explain it.",
               ansi(ANSI_BOLD), ansi(ANSI_RESET));
    }

    if (state->notice[0] != '\0') {
        move_to(about_row + 5, left);
        printf("%s%s%s", ansi(ANSI_YELLOW), state->notice, ansi(ANSI_RESET));
    }

    move_to(footer_row, left);
    print_rule(width);
    move_to(footer_row + 1, left);
    print_key("Up/Down", "Move", ANSI_CYAN);
    printf("  ");
    print_key("Enter", "Details", ANSI_GREEN);
    printf("  ");
    print_key("?", "Explain", ANSI_CYAN);
    printf("  ");
    print_key("k", "Stop safely", ANSI_GREEN);
    printf("  ");
    print_key("K", "Force kill", ANSI_RED);
    printf("  %s[m] Memory  [p] PID  [n] Name  [Esc] Back  [q] Quit%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
}

static void render_process_detail(const struct process_detail *detail, struct terminal_size term)
{
    int left = 3;
    int width = term.cols - 5;
    int footer = term.rows - 2;

    fputs("\033[H\033[J", stdout);
    move_to(1, left);
    printf("%sABOUT%s  %s%s%s",
           ansi(ANSI_BOLD), ansi(ANSI_RESET), ansi(ANSI_CYAN), detail->row.name, ansi(ANSI_RESET));
    move_to(2, left);
    printf("%sA closer look at one running process. Press ? when you want the Linux internals.%s",
           ansi(ANSI_DIM), ansi(ANSI_RESET));
    move_to(3, left);
    print_rule(width);

    move_to(5, left);
    printf("PID        %d", detail->row.pid);
    move_to(6, left);
    printf("State      %s", teaching_state_name(detail->row.state));
    move_to(7, left);
    printf("Memory     %.1f MiB", (double)detail->row.rss_kib / KIB_PER_MIB);
    move_to(8, left);
    printf("Threads    %lu", detail->row.threads);

    move_to(10, left);
    print_section("COMMAND");
    move_to(11, left);
    printf("%.*s", width, detail->command);

    move_to(14, left);
    printf("%sWHAT THIS TELLS YOU%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
    move_to(15, left);
    printf("%s", teaching_state_explanation(detail->row.state));
    move_to(17, left);
    printf("Curious where PID, memory, state, and threads come from? Press %s?%s.",
           ansi(ANSI_BOLD), ansi(ANSI_RESET));

    move_to(footer, left);
    print_rule(width);
    move_to(footer + 1, left);
    print_key("?", "Explain", ANSI_CYAN);
    printf("   ");
    print_key("k", "Stop safely", ANSI_GREEN);
    printf("   ");
    print_key("K", "Force kill", ANSI_RED);
    printf("   %s[Esc] Back  [q] Quit%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
}

static void render_process_learning(const struct process_detail *detail, struct terminal_size term)
{
    int left = 4;
    int width = term.cols - 7;
    int footer = term.rows - 2;

    fputs("\033[H\033[J", stdout);
    move_to(1, left);
    printf("%sHOW LINUX SEES%s  %s%s%s",
           ansi(ANSI_CYAN), ansi(ANSI_RESET), ansi(ANSI_BOLD), detail->row.name, ansi(ANSI_RESET));
    move_to(2, left);
    printf("This explanation uses the process that is running on your machine right now.");
    move_to(3, left);
    print_rule(width);

    move_to(5, left);
    printf("%sTHE IDEA%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
    move_to(6, left);
    printf("A process is one running instance of a program. Linux currently knows this one as PID %d.", detail->row.pid);
    move_to(7, left);
    printf("That PID can be reused later, so RookieTop also remembers when this process started before sending signals.");

    move_to(10, left);
    printf("%sRIGHT NOW%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
    move_to(11, left);
    printf("State: %s. %s", teaching_state_name(detail->row.state), teaching_state_explanation(detail->row.state));
    move_to(12, left);
    printf("Memory: %.1f MiB in physical RAM | Threads: %lu",
           (double)detail->row.rss_kib / KIB_PER_MIB, detail->row.threads);

    move_to(15, left);
    printf("%sWHERE IT COMES FROM%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
    move_to(16, left);
    printf("/proc/%d/status   memory, threads, and readable state", detail->row.pid);
    move_to(17, left);
    printf("/proc/%d/stat     CPU accounting, state, and process start time", detail->row.pid);
    move_to(18, left);
    printf("/proc/%d/cmdline  command and arguments", detail->row.pid);

    move_to(21, left);
    printf("%sTRY IT%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
    move_to(22, left);
    printf("Run:  cat /proc/%d/status", detail->row.pid);
    move_to(23, left);
    printf("Look for State, Threads, and VmRSS, then compare them with RookieTop.");

    move_to(footer, left);
    print_rule(width);
    move_to(footer + 1, left);
    print_key("k", "Stop safely", ANSI_GREEN);
    printf("   ");
    print_key("K", "Force kill", ANSI_RED);
    printf("   %s[Esc] Back  [q] Quit%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
}

static void print_live_context(const struct overview *view, enum teaching_concept concept)
{
    if (concept == TEACH_CPU) {
        printf("CPU is %.1f%% busy right now (%s).", view->cpu_percent, cpu_status(view->cpu_percent));
    } else if (concept == TEACH_MEMORY) {
        printf("Memory is %.1f%% used, with %.1f GiB still available.",
               view->memory_percent, (double)view->memory.available_kib / KIB_PER_GIB);
    } else if (concept == TEACH_LOAD && view->host_ok) {
        printf("Load is %.2f on %ld CPUs (%s).", view->host.load1, view->host.cores, load_status(view));
    } else if (concept == TEACH_DISK) {
        printf("The root filesystem is %.1f%% used, with %.1f GiB available.",
               view->disk_percent, (double)view->disk.available_bytes / BYTES_PER_GIB);
    } else if (concept == TEACH_NETWORK && view->network_ok) {
        printf("Traffic now: down %.2f MiB/s, up %.2f MiB/s.",
               view->rx_per_sec / BYTES_PER_MIB, view->tx_per_sec / BYTES_PER_MIB);
    } else {
        printf("RookieTop connects this concept to Linux interfaces on your running machine.");
    }
}

static void render_learn_menu(const struct app_state *state, struct terminal_size term)
{
    int left = 4;
    int width = term.cols - 7;
    int footer = term.rows - 2;

    fputs("\033[H\033[J", stdout);
    move_to(1, left);
    printf("%sEXPLORE YOUR LINUX SYSTEM%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
    move_to(2, left);
    printf("Pick something you are curious about. The examples come from this machine.");
    move_to(3, left);
    print_rule(width);

    if (state->view_ok) {
        move_to(5, left);
        printf("%sLIVE NOW%s  CPU %.1f%%  |  Memory %.1f%%  |  Disk %.1f%%",
               ansi(ANSI_BOLD), ansi(ANSI_RESET),
               state->view.cpu_percent, state->view.memory_percent, state->view.disk_percent);
        if (state->view.host_ok) {
            printf("  |  Load %.2f / %ld CPUs", state->view.host.load1, state->view.host.cores);
        }
    }

    int row = 8;
    for (size_t i = 0; i < teaching_count(); i++, row += 2) {
        const struct teaching_topic *topic = teaching_get((enum teaching_concept)i);
        if (topic == NULL) {
            continue;
        }
        move_to(row, left);
        if (i == state->learn_selected) {
            printf("%s> %-22s%s  %s", ansi(ANSI_REVERSE), topic->title, ansi(ANSI_RESET), topic->summary);
        } else {
            printf("  %-22s  %s%s%s", topic->title, ansi(ANSI_DIM), topic->summary, ansi(ANSI_RESET));
        }
    }

    move_to(footer, left);
    print_rule(width);
    move_to(footer + 1, left);
    print_key("Up/Down", "Choose", ANSI_CYAN);
    printf("   ");
    print_key("Enter", "Explain", ANSI_GREEN);
    printf("   %s[Esc] Back  [q] Quit%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
}

static int print_wrapped(int row, int col, int width, const char *text)
{
    int current_row = row;
    const char *cursor = text;

    while (*cursor != '\0') {
        while (*cursor == ' ') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        size_t remaining = strlen(cursor);
        size_t take = remaining < (size_t)width ? remaining : (size_t)width;
        if (remaining > (size_t)width) {
            size_t split = take;
            while (split > 0 && cursor[split] != ' ') {
                split--;
            }
            if (split > 0) {
                take = split;
            }
        }

        move_to(current_row++, col);
        printf("%.*s", (int)take, cursor);
        cursor += take;
        while (*cursor == ' ') {
            cursor++;
        }
    }
    return current_row;
}

static void render_lesson(const struct app_state *state, struct terminal_size term)
{
    const struct teaching_topic *topic = teaching_get(state->lesson);
    int left = 4;
    int width = term.cols - 9;
    int footer = term.rows - 2;
    int row = 1;

    if (topic == NULL) {
        return;
    }
    if (width > 110) {
        width = 110;
    }
    if (width < 48) {
        width = 48;
    }

    fputs("\033[H\033[J", stdout);
    move_to(row++, left);
    printf("%sEXPLAIN%s  %s%s%s", ansi(ANSI_CYAN), ansi(ANSI_RESET), ansi(ANSI_BOLD), topic->title, ansi(ANSI_RESET));
    move_to(row++, left);
    printf("%s%s%s", ansi(ANSI_DIM), topic->summary, ansi(ANSI_RESET));
    move_to(row++, left);
    print_rule(term.cols - 7);

    if (state->view_ok) {
        row++;
        move_to(row++, left);
        printf("%sON YOUR MACHINE%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
        move_to(row++, left);
        print_live_context(&state->view, state->lesson);
    }

    row++;
    move_to(row++, left);
    printf("%sWHAT IT MEANS%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
    row = print_wrapped(row, left, width, topic->what);

    row++;
    move_to(row++, left);
    printf("%sWHEN TO CARE%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
    row = print_wrapped(row, left, width, topic->why);

    row++;
    move_to(row++, left);
    printf("%sHOW LINUX SHOWS IT%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
    row = print_wrapped(row, left, width, topic->how);
    move_to(row++, left);
    printf("%sSource: %s%s", ansi(ANSI_DIM), topic->source, ansi(ANSI_RESET));

    row++;
    move_to(row++, left);
    printf("%sTRY IT%s", ansi(ANSI_CYAN), ansi(ANSI_RESET));
    (void)print_wrapped(row, left, width, topic->try_it);

    move_to(footer, left);
    print_rule(term.cols - 7);
    move_to(footer + 1, left);
    print_key("n", "Next", ANSI_CYAN);
    printf("   ");
    print_key("b", "Previous", ANSI_CYAN);
    printf("   %s[Esc] Topics  [q] Quit%s", ansi(ANSI_DIM), ansi(ANSI_RESET));
}

static void render_confirm(const struct process_detail *detail,
                           struct terminal_size term,
                           int force)
{
    int left = 4;
    int width = term.cols - 7;
    int footer = term.rows - 2;

    fputs("\033[H\033[J", stdout);
    move_to(1, left);
    if (force) {
        printf("%sFORCE KILL%s  %s?", ansi(ANSI_RED), ansi(ANSI_RESET), detail->row.name);
    } else {
        printf("%sSTOP%s  %s?", ansi(ANSI_GREEN), ansi(ANSI_RESET), detail->row.name);
    }
    move_to(2, left);
    print_rule(width);

    move_to(5, left);
    if (force) {
        printf("This ends the process immediately. It does not get a chance to save state or clean up first.");
        move_to(7, left);
        printf("%sUse Force Kill only when a normal stop did not work.%s", ansi(ANSI_RED), ansi(ANSI_RESET));
        move_to(10, left);
        printf("Linux signal: SIGKILL");
    } else {
        printf("RookieTop will ask this process to exit cleanly.");
        move_to(7, left);
        printf("It gets a chance to finish work, close files, and clean up before stopping.");
        move_to(10, left);
        printf("Linux signal: SIGTERM");
    }
    move_to(12, left);
    printf("%sRookieTop re-checks PID + start time before sending the signal.%s",
           ansi(ANSI_DIM), ansi(ANSI_RESET));

    move_to(footer, left);
    print_rule(width);
    move_to(footer + 1, left);
    print_key("y", force ? "Force kill" : "Stop it", force ? ANSI_RED : ANSI_GREEN);
    printf("   %s[n/Esc] %s  [q] Quit%s",
           ansi(ANSI_DIM), force ? "Cancel" : "Keep running", ansi(ANSI_RESET));
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
        (void)snprintf(state->notice,
                       sizeof(state->notice),
                       force ? "%s was force-killed with SIGKILL."
                             : "%s was asked to stop cleanly with SIGTERM.",
                       name);
    } else if (result == PROCESS_SIGNAL_PERMISSION) {
        set_notice(state, "RookieTop cannot stop this process with your current permissions.");
    } else if (result == PROCESS_SIGNAL_REUSED) {
        set_notice(state, "That PID now belongs to a different process, so RookieTop cancelled the action.");
    } else if (result == PROCESS_SIGNAL_GONE) {
        set_notice(state, "The process already exited, so there was nothing to stop.");
    } else {
        set_notice(state, "RookieTop could not send the signal.");
    }
}

static int refresh_selected_detail(struct app_state *state)
{
    struct process_detail detail;
    if (process_detail_read(state->selected_pid, &detail) != 0 ||
        detail.row.starttime != state->selected_starttime) {
        return -1;
    }
    state->selected_detail = detail;
    return 0;
}

static void open_selected_detail(struct app_state *state,
                                 const struct process_row *row,
                                 enum app_screen screen)
{
    state->selected_pid = row->pid;
    state->selected_starttime = row->starttime;
    if (process_detail_read(row->pid, &state->selected_detail) == 0 &&
        state->selected_detail.row.starttime == row->starttime) {
        state->screen = screen;
    } else {
        set_notice(state, "The process changed or exited before RookieTop could open it.");
        state->screen = SCREEN_PROCESSES;
    }
}

static void open_confirm_from_detail(struct app_state *state, int force)
{
    if (refresh_selected_detail(state) != 0) {
        set_notice(state, "The process exited or that PID now belongs to something else.");
        state->screen = SCREEN_PROCESSES;
        return;
    }
    state->screen = force ? SCREEN_CONFIRM_KILL : SCREEN_CONFIRM_TERM;
}

static void handle_signal_confirm(struct app_state *state, int force)
{
    if (state->selected_detail.row.pid == (int)getpid()) {
        set_notice(state, "RookieTop will not stop itself.");
        state->screen = SCREEN_PROCESSES;
        return;
    }

    enum process_signal_result result = process_send_signal(state->selected_detail.row.pid,
                                                            state->selected_detail.row.starttime,
                                                            force ? SIGKILL : SIGTERM);
    set_signal_notice(state, result, force, state->selected_detail.row.name);
    state->screen = SCREEN_PROCESSES;
}

static int refresh_overview(struct app_state *state)
{
    if (collect_overview(&state->view) != 0) {
        state->view_ok = 0;
        return -1;
    }
    state->view_ok = 1;
    history_push(&state->view);
    return 0;
}

static void prepare_learn_from_view(struct app_state *state)
{
    state->lesson = diagnosis_lesson(&state->view);
    state->learn_selected = (size_t)state->lesson;
    state->return_screen = SCREEN_OVERVIEW;
    state->screen = SCREEN_LEARN_MENU;
}

static int run_interactive(void)
{
    struct app_state state;
    memset(&state, 0, sizeof(state));
    state.screen = SCREEN_OVERVIEW;
    state.return_screen = SCREEN_OVERVIEW;
    state.sort = SORT_MEMORY;
    state.lesson = TEACH_CPU;

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
            if (refresh_overview(&state) != 0) {
                return 1;
            }
            if (term.cols < MIN_FULL_COLS || term.rows < MIN_FULL_ROWS) {
                fputs("\033[H\033[J", stdout);
                render_compact(&state.view);
                printf("\nResize to at least %dx%d for the full interactive layout.\n", MIN_FULL_COLS, MIN_FULL_ROWS);
            } else {
                render_overview(&state.view, term);
            }
            fflush(stdout);
            key = terminal_input_read_key(FRAME_WAIT_MS);
            if (key == 'p') {
                state.screen = SCREEN_PROCESSES;
                state.notice[0] = '\0';
            } else if (key == '?' || key == 'l' || key == 'L') {
                prepare_learn_from_view(&state);
            }
        } else if (state.screen == SCREEN_PROCESSES) {
            struct process_row *rows = NULL;
            size_t count = 0;
            if (process_list_read(&rows, &count) != 0) {
                set_notice(&state, "RookieTop could not read the process list.");
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
                open_selected_detail(&state, &rows[state.selected], SCREEN_DETAIL);
            } else if (key == '?' && count > 0) {
                open_selected_detail(&state, &rows[state.selected], SCREEN_PROCESS_LEARN);
            } else if (key == 'k' && count > 0) {
                open_selected_detail(&state, &rows[state.selected], SCREEN_CONFIRM_TERM);
            } else if (key == 'K' && count > 0) {
                open_selected_detail(&state, &rows[state.selected], SCREEN_CONFIRM_KILL);
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
            } else if (key == 'l' || key == 'L') {
                state.return_screen = SCREEN_PROCESSES;
                state.screen = SCREEN_LEARN_MENU;
            } else if (key == INPUT_ESCAPE) {
                state.screen = SCREEN_OVERVIEW;
            }
            process_list_free(rows);
        } else if (state.screen == SCREEN_DETAIL) {
            if (refresh_selected_detail(&state) != 0) {
                set_notice(&state, "The process exited or that PID was reused.");
                state.screen = SCREEN_PROCESSES;
                continue;
            }
            render_process_detail(&state.selected_detail, term);
            fflush(stdout);
            key = terminal_input_read_key(PROCESS_WAIT_MS);
            if (key == '?') {
                state.screen = SCREEN_PROCESS_LEARN;
            } else if (key == 'k') {
                open_confirm_from_detail(&state, 0);
            } else if (key == 'K') {
                open_confirm_from_detail(&state, 1);
            } else if (key == INPUT_ESCAPE) {
                state.screen = SCREEN_PROCESSES;
            }
        } else if (state.screen == SCREEN_PROCESS_LEARN) {
            if (refresh_selected_detail(&state) != 0) {
                set_notice(&state, "The process exited or that PID was reused.");
                state.screen = SCREEN_PROCESSES;
                continue;
            }
            render_process_learning(&state.selected_detail, term);
            fflush(stdout);
            key = terminal_input_read_key(PROCESS_WAIT_MS);
            if (key == 'k') {
                open_confirm_from_detail(&state, 0);
            } else if (key == 'K') {
                open_confirm_from_detail(&state, 1);
            } else if (key == INPUT_ESCAPE) {
                state.screen = SCREEN_PROCESSES;
            }
        } else if (state.screen == SCREEN_LEARN_MENU) {
            render_learn_menu(&state, term);
            fflush(stdout);
            key = terminal_input_read_key(-1);
            if (key == INPUT_UP && state.learn_selected > 0) {
                state.learn_selected--;
            } else if (key == INPUT_DOWN && state.learn_selected + 1 < teaching_count()) {
                state.learn_selected++;
            } else if (key == INPUT_ENTER) {
                state.lesson = (enum teaching_concept)state.learn_selected;
                state.screen = SCREEN_LEARN_TOPIC;
            } else if (key == INPUT_ESCAPE) {
                state.screen = state.return_screen;
            }
        } else if (state.screen == SCREEN_LEARN_TOPIC) {
            render_lesson(&state, term);
            fflush(stdout);
            key = terminal_input_read_key(-1);
            if (key == 'n' || key == 'N') {
                state.lesson = (enum teaching_concept)(((int)state.lesson + 1) % TEACH_COUNT);
                state.learn_selected = (size_t)state.lesson;
            } else if (key == 'b' || key == 'B') {
                state.lesson = (enum teaching_concept)(((int)state.lesson + TEACH_COUNT - 1) % TEACH_COUNT);
                state.learn_selected = (size_t)state.lesson;
            } else if (key == INPUT_ESCAPE) {
                state.screen = SCREEN_LEARN_MENU;
            }
        } else {
            int force = state.screen == SCREEN_CONFIRM_KILL;
            if (refresh_selected_detail(&state) != 0) {
                set_notice(&state, "The process exited or that PID was reused.");
                state.screen = SCREEN_PROCESSES;
                continue;
            }
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
        struct overview view;
        if (collect_overview(&view) != 0) {
            fputs("rookietop: could not read system data\n", stderr);
            return 1;
        }
        render_compact(&view);
        return 0;
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

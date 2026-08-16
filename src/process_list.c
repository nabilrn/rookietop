#define _POSIX_C_SOURCE 200809L

#include "process_list.h"
#include "process_cpu.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROC_ROOT "/proc"
#define PATH_MAX_LOCAL 64
#define LINE_MAX_LOCAL 1024
#define INITIAL_CAPACITY 128

static int parse_pid(const char *text, int *out)
{
    if (text == NULL || *text == '\0' || out == NULL) {
        return -1;
    }

    errno = 0;
    char *end;
    long value = strtol(text, &end, 10);
    if (errno != 0 || *end != '\0' || value <= 0 || value > INT_MAX) {
        return -1;
    }

    *out = (int)value;
    return 0;
}

static void trim_newline(char *text)
{
    if (text != NULL) {
        text[strcspn(text, "\r\n")] = '\0';
    }
}

static int parse_status_line(struct process_row *row, const char *line)
{
    if (strncmp(line, "Name:", 5) == 0) {
        const char *value = line + 5;
        while (*value == ' ' || *value == '\t') {
            value++;
        }
        size_t len = strcspn(value, "\r\n");
        if (len == 0 || len >= sizeof(row->name)) {
            return -1;
        }
        memcpy(row->name, value, len);
        row->name[len] = '\0';
        return 1;
    }

    if (strncmp(line, "State:", 6) == 0) {
        const char *value = line + 6;
        while (*value == ' ' || *value == '\t') {
            value++;
        }
        if (*value == '\0' || *value == '\n') {
            return -1;
        }
        row->state = *value;
        return 1;
    }

    if (strncmp(line, "Threads:", 8) == 0) {
        unsigned long threads;
        char extra;
        if (sscanf(line + 8, " %lu %c", &threads, &extra) != 1) {
            return -1;
        }
        row->threads = threads;
        return 1;
    }

    if (strncmp(line, "VmRSS:", 6) == 0) {
        unsigned long long rss;
        char unit[3] = {0};
        char extra;
        if (sscanf(line + 6, " %llu %2s %c", &rss, unit, &extra) != 2 ||
            strcmp(unit, "kB") != 0) {
            return -1;
        }
        row->rss_kib = (uint64_t)rss;
        return 1;
    }

    return 0;
}

static int read_starttime(int pid, uint64_t *out)
{
    char path[PATH_MAX_LOCAL];
    int written = snprintf(path, sizeof(path), PROC_ROOT "/%d/stat", pid);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        return -1;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    char line[LINE_MAX_LOCAL];
    char *read = fgets(line, sizeof(line), file);
    int close_result = fclose(file);
    if (read == NULL || close_result != 0) {
        return -1;
    }

    struct process_cpu_stat stat;
    if (process_cpu_parse_stat(line, &stat) != 0) {
        return -1;
    }

    *out = stat.starttime;
    return 0;
}

static int read_row(int pid, struct process_row *out)
{
    char path[PATH_MAX_LOCAL];
    int written = snprintf(path, sizeof(path), PROC_ROOT "/%d/status", pid);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        return -1;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    struct process_row row = {.pid = pid, .state = '?', .cpu_percent = -1.0};
    char line[LINE_MAX_LOCAL];
    int found_name = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        int parsed = parse_status_line(&row, line);
        if (parsed < 0) {
            (void)fclose(file);
            return -1;
        }
        if (parsed > 0 && strncmp(line, "Name:", 5) == 0) {
            found_name = 1;
        }
    }

    int error = ferror(file);
    int close_result = fclose(file);
    if (error != 0 || close_result != 0 || !found_name ||
        read_starttime(pid, &row.starttime) != 0) {
        return -1;
    }

    *out = row;
    return 0;
}

int process_list_read(struct process_row **out, size_t *out_count)
{
    if (out == NULL || out_count == NULL) {
        return -1;
    }

    *out = NULL;
    *out_count = 0;

    DIR *dir = opendir(PROC_ROOT);
    if (dir == NULL) {
        return -1;
    }

    size_t capacity = INITIAL_CAPACITY;
    struct process_row *rows = malloc(capacity * sizeof(rows[0]));
    if (rows == NULL) {
        closedir(dir);
        return -1;
    }

    size_t count = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (entry == NULL) {
            if (errno != 0) {
                free(rows);
                closedir(dir);
                return -1;
            }
            break;
        }

        int pid;
        if (parse_pid(entry->d_name, &pid) != 0) {
            continue;
        }

        struct process_row row;
        if (read_row(pid, &row) != 0) {
            continue;
        }

        if (count == capacity) {
            if (capacity > SIZE_MAX / 2 / sizeof(rows[0])) {
                free(rows);
                closedir(dir);
                return -1;
            }
            capacity *= 2;
            void *grown = realloc(rows, capacity * sizeof(rows[0]));
            if (grown == NULL) {
                free(rows);
                closedir(dir);
                return -1;
            }
            rows = grown;
        }

        rows[count++] = row;
    }

    if (closedir(dir) != 0) {
        free(rows);
        return -1;
    }

    if (count == 0) {
        free(rows);
        rows = NULL;
    }

    *out = rows;
    *out_count = count;
    return 0;
}

void process_list_free(struct process_row *rows)
{
    free(rows);
}

static int read_cmdline(int pid, char *out, size_t cap)
{
    char path[PATH_MAX_LOCAL];
    int written = snprintf(path, sizeof(path), PROC_ROOT "/%d/cmdline", pid);
    if (written < 0 || (size_t)written >= sizeof(path) || cap == 0) {
        return -1;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }

    size_t count = fread(out, 1, cap - 1, file);
    int error = ferror(file);
    int close_result = fclose(file);
    if (error != 0 || close_result != 0) {
        return -1;
    }

    if (count == 0) {
        out[0] = '\0';
        return 0;
    }

    for (size_t i = 0; i < count; i++) {
        if (out[i] == '\0') {
            out[i] = ' ';
        }
    }
    out[count] = '\0';

    while (count > 0 && isspace((unsigned char)out[count - 1])) {
        out[--count] = '\0';
    }

    trim_newline(out);
    return 0;
}

int process_detail_read(int pid, struct process_detail *out)
{
    if (pid <= 0 || out == NULL) {
        return -1;
    }

    struct process_detail detail;
    memset(&detail, 0, sizeof(detail));
    if (read_row(pid, &detail.row) != 0) {
        return -1;
    }

    if (read_cmdline(pid, detail.command, sizeof(detail.command)) != 0 ||
        detail.command[0] == '\0') {
        (void)snprintf(detail.command, sizeof(detail.command), "[%s]", detail.row.name);
    }

    *out = detail;
    return 0;
}

int process_same_instance_alive(int pid, uint64_t starttime)
{
    if (pid <= 0 || starttime == 0) {
        return -1;
    }

    uint64_t current;
    if (read_starttime(pid, &current) != 0) {
        return 0;
    }
    return current == starttime ? 1 : 0;
}

enum process_signal_result process_send_signal(int pid, uint64_t starttime, int signal_number)
{
    if (pid <= 0 || starttime == 0 || (signal_number != SIGTERM && signal_number != SIGKILL)) {
        return PROCESS_SIGNAL_ERROR;
    }

    uint64_t current;
    if (read_starttime(pid, &current) != 0) {
        return PROCESS_SIGNAL_GONE;
    }
    if (current != starttime) {
        return PROCESS_SIGNAL_REUSED;
    }

    if (kill(pid, signal_number) == 0) {
        return PROCESS_SIGNAL_OK;
    }
    if (errno == ESRCH) {
        return PROCESS_SIGNAL_GONE;
    }
    if (errno == EPERM) {
        return PROCESS_SIGNAL_PERMISSION;
    }
    return PROCESS_SIGNAL_ERROR;
}

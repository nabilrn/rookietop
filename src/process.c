#define _POSIX_C_SOURCE 200809L

#include "process.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROC_PATH "/proc"
#define STATUS_LINE_MAX 256
#define STATUS_PATH_MAX 64

static int parse_name(const char *line, char *out, size_t cap)
{
    if (strncmp(line, "Name:", 5) != 0) {
        return 0;
    }

    const char *start = line + 5;
    while (*start == ' ' || *start == '\t') {
        start++;
    }

    const char *end = start + strcspn(start, "\r\n");
    while (end > start && isspace((unsigned char)end[-1])) {
        end--;
    }

    size_t len = (size_t)(end - start);
    if (len == 0 || len >= cap) {
        return -1;
    }

    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

static int parse_rss(const char *line, uint64_t *out)
{
    if (strncmp(line, "VmRSS:", 6) != 0) {
        return 0;
    }

    uint64_t value;
    char unit[3] = {0};
    char extra;
    int fields = sscanf(line + 6, " %" SCNu64 " %2s %c", &value, unit, &extra);

    if (fields != 2 || strcmp(unit, "kB") != 0) {
        return -1;
    }

    *out = value;
    return 1;
}

int process_parse_status(FILE *file, int pid, struct process_info *out)
{
    if (file == NULL || out == NULL || pid <= 0) {
        return -1;
    }

    struct process_info process = {.pid = pid, .name = {0}, .rss_kib = 0};
    int found_name = 0;
    char line[STATUS_LINE_MAX];

    while (fgets(line, sizeof(line), file) != NULL) {
        int result = parse_name(line, process.name, sizeof(process.name));
        if (result < 0) {
            return -1;
        }
        if (result > 0) {
            found_name = 1;
            continue;
        }

        result = parse_rss(line, &process.rss_kib);
        if (result < 0) {
            return -1;
        }
    }

    if (ferror(file) != 0 || !found_name) {
        return -1;
    }

    *out = process;
    return 0;
}

static int parse_pid(const char *name, int *out)
{
    if (name == NULL || *name == '\0' || out == NULL) {
        return -1;
    }

    errno = 0;
    char *end;
    long value = strtol(name, &end, 10);

    if (errno != 0 || *end != '\0' || value <= 0 || value > INT_MAX) {
        return -1;
    }

    *out = (int)value;
    return 0;
}

static void insert_top(struct process_info *out,
                       size_t cap,
                       size_t *count,
                       const struct process_info *candidate)
{
    size_t pos = 0;
    while (pos < *count && out[pos].rss_kib >= candidate->rss_kib) {
        pos++;
    }

    if (pos >= cap) {
        return;
    }

    size_t new_count = *count < cap ? *count + 1 : *count;
    if (new_count > pos + 1) {
        memmove(&out[pos + 1],
                &out[pos],
                (new_count - pos - 1) * sizeof(out[0]));
    }

    out[pos] = *candidate;
    *count = new_count;
}

int process_top_memory(struct process_info *out,
                       size_t cap,
                       size_t *out_count,
                       size_t *out_total)
{
    if (out == NULL || cap == 0 || out_count == NULL || out_total == NULL) {
        return -1;
    }

    DIR *dir = opendir(PROC_PATH);
    if (dir == NULL) {
        return -1;
    }

    size_t count = 0;
    size_t total = 0;

    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (entry == NULL) {
            if (errno != 0) {
                closedir(dir);
                return -1;
            }
            break;
        }

        int pid;
        if (parse_pid(entry->d_name, &pid) != 0) {
            continue;
        }

        char path[STATUS_PATH_MAX];
        int written = snprintf(path, sizeof(path), PROC_PATH "/%d/status", pid);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            continue;
        }

        FILE *file = fopen(path, "r");
        if (file == NULL) {
            continue;
        }

        struct process_info process;
        int result = process_parse_status(file, pid, &process);
        int close_result = fclose(file);

        if (result != 0 || close_result != 0) {
            continue;
        }

        total++;
        insert_top(out, cap, &count, &process);
    }

    if (closedir(dir) != 0) {
        return -1;
    }

    *out_count = count;
    *out_total = total;
    return 0;
}

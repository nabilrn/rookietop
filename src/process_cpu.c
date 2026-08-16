#define _POSIX_C_SOURCE 200809L

#include "process_cpu.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROC_ROOT "/proc"
#define PROC_STAT_PATH_MAX 64
#define PROC_STAT_LINE_MAX 1024
#define INITIAL_CAPACITY 128

struct process_cpu_counter {
    int pid;
    uint64_t ticks;
    uint64_t starttime;
};

struct process_cpu_snapshot {
    struct process_cpu_counter *items;
    size_t count;
};

static int parse_uint_token(const char *start, size_t len, uint64_t *out)
{
    if (start == NULL || out == NULL || len == 0 || len >= 32) {
        return -1;
    }

    char buffer[32];
    memcpy(buffer, start, len);
    buffer[len] = '\0';

    errno = 0;
    char *end;
    unsigned long long value = strtoull(buffer, &end, 10);
    if (errno != 0 || *end != '\0') {
        return -1;
    }

    *out = (uint64_t)value;
    return 0;
}

int process_cpu_parse_stat(const char *line, struct process_cpu_stat *out)
{
    if (line == NULL || out == NULL) {
        return -1;
    }

    errno = 0;
    char *pid_end;
    long pid_value = strtol(line, &pid_end, 10);
    if (errno != 0 || pid_end == line || pid_value <= 0 || pid_value > INT_MAX) {
        return -1;
    }

    const char *open = strchr(pid_end, '(');
    const char *close = strrchr(line, ')');
    if (open == NULL || close == NULL || close <= open + 1) {
        return -1;
    }

    size_t name_len = (size_t)(close - open - 1);
    if (name_len >= PROCESS_CPU_NAME_MAX) {
        return -1;
    }

    struct process_cpu_stat stat = {.pid = (int)pid_value};
    memcpy(stat.name, open + 1, name_len);
    stat.name[name_len] = '\0';

    const char *cursor = close + 1;
    uint64_t utime = 0;
    uint64_t stime = 0;
    int found_utime = 0;
    int found_stime = 0;
    int found_starttime = 0;

    for (int field = 3; field <= 22; field++) {
        while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (*cursor == '\0') {
            return -1;
        }

        const char *token = cursor;
        while (*cursor != '\0' && !isspace((unsigned char)*cursor)) {
            cursor++;
        }
        size_t len = (size_t)(cursor - token);

        if (field == 14) {
            if (parse_uint_token(token, len, &utime) != 0) {
                return -1;
            }
            found_utime = 1;
        } else if (field == 15) {
            if (parse_uint_token(token, len, &stime) != 0) {
                return -1;
            }
            found_stime = 1;
        } else if (field == 22) {
            if (parse_uint_token(token, len, &stat.starttime) != 0) {
                return -1;
            }
            found_starttime = 1;
        }
    }

    if (!found_utime || !found_stime || !found_starttime || UINT64_MAX - utime < stime) {
        return -1;
    }

    stat.ticks = utime + stime;
    *out = stat;
    return 0;
}

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

static int read_process_stat(int pid, struct process_cpu_stat *out)
{
    char path[PROC_STAT_PATH_MAX];
    int written = snprintf(path, sizeof(path), PROC_ROOT "/%d/stat", pid);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        return -1;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    char line[PROC_STAT_LINE_MAX];
    char *read = fgets(line, sizeof(line), file);
    int close_result = fclose(file);
    if (read == NULL || close_result != 0) {
        return -1;
    }

    return process_cpu_parse_stat(line, out);
}

static int compare_counter(const void *a, const void *b)
{
    const struct process_cpu_counter *left = a;
    const struct process_cpu_counter *right = b;
    return (left->pid > right->pid) - (left->pid < right->pid);
}

struct process_cpu_snapshot *process_cpu_snapshot_take(void)
{
    DIR *dir = opendir(PROC_ROOT);
    if (dir == NULL) {
        return NULL;
    }

    struct process_cpu_snapshot *snapshot = calloc(1, sizeof(*snapshot));
    if (snapshot == NULL) {
        closedir(dir);
        return NULL;
    }

    size_t capacity = INITIAL_CAPACITY;
    snapshot->items = malloc(capacity * sizeof(snapshot->items[0]));
    if (snapshot->items == NULL) {
        free(snapshot);
        closedir(dir);
        return NULL;
    }

    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (entry == NULL) {
            if (errno != 0) {
                process_cpu_snapshot_free(snapshot);
                closedir(dir);
                return NULL;
            }
            break;
        }

        int pid;
        if (parse_pid(entry->d_name, &pid) != 0) {
            continue;
        }

        struct process_cpu_stat stat;
        if (read_process_stat(pid, &stat) != 0) {
            continue;
        }

        if (snapshot->count == capacity) {
            if (capacity > SIZE_MAX / 2 / sizeof(snapshot->items[0])) {
                process_cpu_snapshot_free(snapshot);
                closedir(dir);
                return NULL;
            }
            capacity *= 2;
            void *grown = realloc(snapshot->items, capacity * sizeof(snapshot->items[0]));
            if (grown == NULL) {
                process_cpu_snapshot_free(snapshot);
                closedir(dir);
                return NULL;
            }
            snapshot->items = grown;
        }

        snapshot->items[snapshot->count++] = (struct process_cpu_counter){
            .pid = stat.pid,
            .ticks = stat.ticks,
            .starttime = stat.starttime,
        };
    }

    if (closedir(dir) != 0) {
        process_cpu_snapshot_free(snapshot);
        return NULL;
    }

    qsort(snapshot->items, snapshot->count, sizeof(snapshot->items[0]), compare_counter);
    return snapshot;
}

void process_cpu_snapshot_free(struct process_cpu_snapshot *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    free(snapshot->items);
    free(snapshot);
}

static const struct process_cpu_counter *find_previous(const struct process_cpu_snapshot *snapshot,
                                                       int pid)
{
    size_t low = 0;
    size_t high = snapshot->count;

    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (snapshot->items[mid].pid == pid) {
            return &snapshot->items[mid];
        }
        if (snapshot->items[mid].pid < pid) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return NULL;
}

static void insert_top(struct process_cpu_info *out,
                       size_t cap,
                       size_t *count,
                       const struct process_cpu_info *candidate)
{
    size_t pos = 0;
    while (pos < *count && out[pos].cpu_percent >= candidate->cpu_percent) {
        pos++;
    }
    if (pos >= cap) {
        return;
    }

    size_t new_count = *count < cap ? *count + 1 : *count;
    if (new_count > pos + 1) {
        memmove(&out[pos + 1], &out[pos], (new_count - pos - 1) * sizeof(out[0]));
    }
    out[pos] = *candidate;
    *count = new_count;
}

int process_cpu_top_since(const struct process_cpu_snapshot *previous,
                          uint64_t system_delta_ticks,
                          struct process_cpu_info *out,
                          size_t cap,
                          size_t *out_count,
                          size_t *out_total)
{
    if (previous == NULL || system_delta_ticks == 0 || out == NULL || cap == 0 ||
        out_count == NULL || out_total == NULL) {
        return -1;
    }

    DIR *dir = opendir(PROC_ROOT);
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

        struct process_cpu_stat current;
        if (read_process_stat(pid, &current) != 0) {
            continue;
        }
        total++;

        const struct process_cpu_counter *old = find_previous(previous, pid);
        if (old == NULL || old->starttime != current.starttime || current.ticks < old->ticks) {
            continue;
        }

        uint64_t delta_ticks = current.ticks - old->ticks;
        if (delta_ticks == 0) {
            continue;
        }

        struct process_cpu_info candidate = {
            .pid = current.pid,
            .cpu_percent = (double)delta_ticks * 100.0 / (double)system_delta_ticks,
        };
        memcpy(candidate.name, current.name, sizeof(candidate.name));
        candidate.name[sizeof(candidate.name) - 1] = '\0';
        insert_top(out, cap, &count, &candidate);
    }

    if (closedir(dir) != 0) {
        return -1;
    }

    *out_count = count;
    *out_total = total;
    return 0;
}

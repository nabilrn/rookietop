#include "memory.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define PROC_MEMINFO_PATH "/proc/meminfo"
#define MEMINFO_LINE_MAX 256

static int read_kib(const char *line, const char *name, uint64_t *out)
{
    size_t name_len = strlen(name);

    if (strncmp(line, name, name_len) != 0 || line[name_len] != ':') {
        return 0;
    }

    uint64_t value;
    char unit[3] = {0};
    char extra;
    int fields = sscanf(line + name_len + 1,
                        " %" SCNu64 " %2s %c",
                        &value,
                        unit,
                        &extra);

    if (fields != 2 || strcmp(unit, "kB") != 0) {
        return -1;
    }

    *out = value;
    return 1;
}

int memory_parse(FILE *file, struct memory_sample *out)
{
    enum {
        FOUND_TOTAL = 1u << 0,
        FOUND_AVAILABLE = 1u << 1,
        FOUND_SWAP_TOTAL = 1u << 2,
        FOUND_SWAP_FREE = 1u << 3,
        FOUND_ALL = FOUND_TOTAL | FOUND_AVAILABLE | FOUND_SWAP_TOTAL | FOUND_SWAP_FREE
    };

    if (file == NULL || out == NULL) {
        return -1;
    }

    struct memory_sample sample = {0};
    unsigned int found = 0;
    char line[MEMINFO_LINE_MAX];

    while (fgets(line, sizeof(line), file) != NULL) {
        int result = read_kib(line, "MemTotal", &sample.total_kib);
        if (result < 0) {
            return -1;
        }
        if (result > 0) {
            found |= FOUND_TOTAL;
            continue;
        }

        result = read_kib(line, "MemAvailable", &sample.available_kib);
        if (result < 0) {
            return -1;
        }
        if (result > 0) {
            found |= FOUND_AVAILABLE;
            continue;
        }

        result = read_kib(line, "SwapTotal", &sample.swap_total_kib);
        if (result < 0) {
            return -1;
        }
        if (result > 0) {
            found |= FOUND_SWAP_TOTAL;
            continue;
        }

        result = read_kib(line, "SwapFree", &sample.swap_free_kib);
        if (result < 0) {
            return -1;
        }
        if (result > 0) {
            found |= FOUND_SWAP_FREE;
        }

        if (found == FOUND_ALL) {
            break;
        }
    }

    if (ferror(file) != 0 || found != FOUND_ALL || sample.total_kib == 0 ||
        sample.available_kib > sample.total_kib ||
        sample.swap_free_kib > sample.swap_total_kib) {
        return -1;
    }

    *out = sample;
    return 0;
}

int memory_read(struct memory_sample *out)
{
    if (out == NULL) {
        return -1;
    }

    FILE *file = fopen(PROC_MEMINFO_PATH, "r");
    if (file == NULL) {
        return -1;
    }

    int result = memory_parse(file, out);
    int close_result = fclose(file);

    if (close_result != 0) {
        return -1;
    }

    return result;
}

int memory_usage(const struct memory_sample *sample,
                 double *out_percent,
                 uint64_t *out_used_kib,
                 uint64_t *out_swap_used_kib)
{
    if (sample == NULL || out_percent == NULL || out_used_kib == NULL ||
        out_swap_used_kib == NULL || sample->total_kib == 0 ||
        sample->available_kib > sample->total_kib ||
        sample->swap_free_kib > sample->swap_total_kib) {
        return -1;
    }

    uint64_t used = sample->total_kib - sample->available_kib;
    uint64_t swap_used = sample->swap_total_kib - sample->swap_free_kib;

    *out_percent = (double)used * 100.0 / (double)sample->total_kib;
    *out_used_kib = used;
    *out_swap_used_kib = swap_used;
    return 0;
}

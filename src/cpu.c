#include "cpu.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define PROC_STAT_PATH "/proc/stat"
#define CPU_LINE_MAX 512

int cpu_parse_line(const char *line, struct cpu_sample *out)
{
    if (line == NULL || out == NULL) {
        return -1;
    }

    struct cpu_sample sample = {0};
    int fields = sscanf(line,
                        "cpu %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
                        " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
                        &sample.user,
                        &sample.nice,
                        &sample.system,
                        &sample.idle,
                        &sample.iowait,
                        &sample.irq,
                        &sample.softirq,
                        &sample.steal);

    if (fields < 4) {
        return -1;
    }

    *out = sample;
    return 0;
}

int cpu_read(struct cpu_sample *out)
{
    if (out == NULL) {
        return -1;
    }

    FILE *file = fopen(PROC_STAT_PATH, "r");
    if (file == NULL) {
        return -1;
    }

    char line[CPU_LINE_MAX];
    char *read = fgets(line, sizeof(line), file);
    int close_result = fclose(file);

    if (read == NULL || close_result != 0) {
        return -1;
    }

    return cpu_parse_line(line, out);
}

static int delta(uint64_t prev, uint64_t curr, uint64_t *out)
{
    if (curr < prev) {
        return -1;
    }

    *out = curr - prev;
    return 0;
}

int cpu_usage(const struct cpu_sample *prev,
              const struct cpu_sample *curr,
              double *out_percent)
{
    if (prev == NULL || curr == NULL || out_percent == NULL) {
        return -1;
    }

    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;

    if (delta(prev->user, curr->user, &user) != 0 ||
        delta(prev->nice, curr->nice, &nice) != 0 ||
        delta(prev->system, curr->system, &system) != 0 ||
        delta(prev->idle, curr->idle, &idle) != 0 ||
        delta(prev->iowait, curr->iowait, &iowait) != 0 ||
        delta(prev->irq, curr->irq, &irq) != 0 ||
        delta(prev->softirq, curr->softirq, &softirq) != 0 ||
        delta(prev->steal, curr->steal, &steal) != 0) {
        return -1;
    }

    uint64_t active = user + nice + system + irq + softirq + steal;
    uint64_t inactive = idle + iowait;
    uint64_t total = active + inactive;

    if (total == 0) {
        return -1;
    }

    *out_percent = (double)active * 100.0 / (double)total;
    return 0;
}

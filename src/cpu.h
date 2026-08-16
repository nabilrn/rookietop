#ifndef ROOKIETOP_CPU_H
#define ROOKIETOP_CPU_H

#include <stdint.h>

struct cpu_sample {
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
};

int cpu_parse_line(const char *line, struct cpu_sample *out);
int cpu_read(struct cpu_sample *out);
int cpu_total_delta(const struct cpu_sample *prev,
                    const struct cpu_sample *curr,
                    uint64_t *out_total);
int cpu_usage(const struct cpu_sample *prev,
              const struct cpu_sample *curr,
              double *out_percent);

#endif

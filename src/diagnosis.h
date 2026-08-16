#ifndef ROOKIETOP_DIAGNOSIS_H
#define ROOKIETOP_DIAGNOSIS_H

#include <stddef.h>
#include <stdint.h>

enum diagnosis_focus {
    DIAGNOSIS_HEALTHY = 0,
    DIAGNOSIS_CPU,
    DIAGNOSIS_MEMORY,
    DIAGNOSIS_DISK,
    DIAGNOSIS_LOAD,
};

struct diagnosis_input {
    double cpu_percent;
    double memory_percent;
    double disk_percent;
    double load_ratio;
    uint64_t memory_available_kib;
    uint64_t disk_available_bytes;
    const char *top_cpu_name;
    double top_cpu_percent;
    const char *top_memory_name;
    uint64_t top_memory_kib;
};

struct diagnosis {
    enum diagnosis_focus focus;
    char headline[128];
    char detail[256];
};

int diagnosis_build(const struct diagnosis_input *input, struct diagnosis *out);

#endif

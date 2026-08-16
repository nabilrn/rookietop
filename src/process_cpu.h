#ifndef ROOKIETOP_PROCESS_CPU_H
#define ROOKIETOP_PROCESS_CPU_H

#include <stddef.h>
#include <stdint.h>

#define PROCESS_CPU_NAME_MAX 64

struct process_cpu_stat {
    int pid;
    char name[PROCESS_CPU_NAME_MAX];
    uint64_t ticks;
    uint64_t starttime;
};

struct process_cpu_info {
    int pid;
    char name[PROCESS_CPU_NAME_MAX];
    double cpu_percent;
    uint64_t starttime;
};

struct process_cpu_snapshot;

int process_cpu_parse_stat(const char *line, struct process_cpu_stat *out);
struct process_cpu_snapshot *process_cpu_snapshot_take(void);
void process_cpu_snapshot_free(struct process_cpu_snapshot *snapshot);
int process_cpu_all_since(const struct process_cpu_snapshot *previous,
                          uint64_t system_delta_ticks,
                          struct process_cpu_info **out,
                          size_t *out_count);
void process_cpu_info_free(struct process_cpu_info *items);
int process_cpu_top_since(const struct process_cpu_snapshot *previous,
                          uint64_t system_delta_ticks,
                          struct process_cpu_info *out,
                          size_t cap,
                          size_t *out_count,
                          size_t *out_total);

#endif

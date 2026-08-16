#ifndef ROOKIETOP_PROCESS_H
#define ROOKIETOP_PROCESS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define PROCESS_NAME_MAX 64

struct process_info {
    int pid;
    char name[PROCESS_NAME_MAX];
    uint64_t rss_kib;
};

int process_parse_status(FILE *file, int pid, struct process_info *out);
int process_top_memory(struct process_info *out,
                       size_t cap,
                       size_t *out_count,
                       size_t *out_total);

#endif

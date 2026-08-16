#ifndef ROOKIETOP_MEMORY_H
#define ROOKIETOP_MEMORY_H

#include <stdint.h>
#include <stdio.h>

struct memory_sample {
    uint64_t total_kib;
    uint64_t available_kib;
    uint64_t swap_total_kib;
    uint64_t swap_free_kib;
};

int memory_parse(FILE *file, struct memory_sample *out);
int memory_read(struct memory_sample *out);
int memory_usage(const struct memory_sample *sample,
                 double *out_percent,
                 uint64_t *out_used_kib,
                 uint64_t *out_swap_used_kib);

#endif

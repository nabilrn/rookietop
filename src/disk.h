#ifndef ROOKIETOP_DISK_H
#define ROOKIETOP_DISK_H

#include <stdint.h>

struct disk_sample {
    uint64_t total_bytes;
    uint64_t available_bytes;
};

int disk_read_root(struct disk_sample *out);
int disk_usage(const struct disk_sample *sample,
               double *out_percent,
               uint64_t *out_used_bytes);

#endif

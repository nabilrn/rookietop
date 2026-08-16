#include "disk.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/statvfs.h>

static int blocks_to_bytes(uint64_t blocks, uint64_t block_size, uint64_t *out)
{
    if (out == NULL || block_size == 0 ||
        (blocks != 0 && block_size > UINT64_MAX / blocks)) {
        return -1;
    }

    *out = blocks * block_size;
    return 0;
}

int disk_read_root(struct disk_sample *out)
{
    if (out == NULL) {
        return -1;
    }

    struct statvfs fs;
    if (statvfs("/", &fs) != 0) {
        return -1;
    }

    uint64_t block_size = (uint64_t)(fs.f_frsize != 0 ? fs.f_frsize : fs.f_bsize);
    struct disk_sample sample;

    if (blocks_to_bytes((uint64_t)fs.f_blocks, block_size, &sample.total_bytes) != 0 ||
        blocks_to_bytes((uint64_t)fs.f_bavail, block_size, &sample.available_bytes) != 0 ||
        sample.total_bytes == 0 || sample.available_bytes > sample.total_bytes) {
        return -1;
    }

    *out = sample;
    return 0;
}

int disk_usage(const struct disk_sample *sample,
               double *out_percent,
               uint64_t *out_used_bytes)
{
    if (sample == NULL || out_percent == NULL || out_used_bytes == NULL ||
        sample->total_bytes == 0 || sample->available_bytes > sample->total_bytes) {
        return -1;
    }

    *out_used_bytes = sample->total_bytes - sample->available_bytes;
    *out_percent = (double)(*out_used_bytes) * 100.0 / (double)sample->total_bytes;
    return 0;
}

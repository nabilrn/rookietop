#include "network.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define PROC_NET_DEV_PATH "/proc/net/dev"
#define NET_LINE_MAX 512

int network_parse_line(const char *line,
                       char *name,
                       size_t name_cap,
                       uint64_t *rx_bytes,
                       uint64_t *tx_bytes)
{
    if (line == NULL || name == NULL || name_cap == 0 ||
        rx_bytes == NULL || tx_bytes == NULL) {
        return -1;
    }

    const char *colon = strchr(line, ':');
    if (colon == NULL) {
        return -1;
    }

    const char *start = line;
    while (*start == ' ' || *start == '\t') {
        start++;
    }

    const char *end = colon;
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }

    size_t len = (size_t)(end - start);
    if (len == 0 || len >= name_cap) {
        return -1;
    }

    memcpy(name, start, len);
    name[len] = '\0';

    uint64_t fields[16];
    int count = sscanf(colon + 1,
                       " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
                       " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
                       " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
                       " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
                       &fields[0], &fields[1], &fields[2], &fields[3],
                       &fields[4], &fields[5], &fields[6], &fields[7],
                       &fields[8], &fields[9], &fields[10], &fields[11],
                       &fields[12], &fields[13], &fields[14], &fields[15]);

    if (count < 9) {
        return -1;
    }

    *rx_bytes = fields[0];
    *tx_bytes = fields[8];
    return 0;
}

int network_read(struct network_sample *out)
{
    if (out == NULL) {
        return -1;
    }

    FILE *file = fopen(PROC_NET_DEV_PATH, "r");
    if (file == NULL) {
        return -1;
    }

    struct network_sample sample = {0};
    char line[NET_LINE_MAX];

    while (fgets(line, sizeof(line), file) != NULL) {
        char name[64];
        uint64_t rx;
        uint64_t tx;

        if (network_parse_line(line, name, sizeof(name), &rx, &tx) != 0) {
            continue;
        }
        if (strcmp(name, "lo") == 0) {
            continue;
        }
        if (UINT64_MAX - sample.rx_bytes < rx || UINT64_MAX - sample.tx_bytes < tx) {
            fclose(file);
            return -1;
        }

        sample.rx_bytes += rx;
        sample.tx_bytes += tx;
    }

    int failed = ferror(file);
    int close_result = fclose(file);
    if (failed != 0 || close_result != 0) {
        return -1;
    }

    *out = sample;
    return 0;
}

int network_rate(const struct network_sample *prev,
                 const struct network_sample *curr,
                 double seconds,
                 double *rx_per_sec,
                 double *tx_per_sec)
{
    if (prev == NULL || curr == NULL || rx_per_sec == NULL || tx_per_sec == NULL ||
        seconds <= 0.0 || curr->rx_bytes < prev->rx_bytes || curr->tx_bytes < prev->tx_bytes) {
        return -1;
    }

    *rx_per_sec = (double)(curr->rx_bytes - prev->rx_bytes) / seconds;
    *tx_per_sec = (double)(curr->tx_bytes - prev->tx_bytes) / seconds;
    return 0;
}

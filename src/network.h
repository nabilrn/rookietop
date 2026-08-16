#ifndef ROOKIETOP_NETWORK_H
#define ROOKIETOP_NETWORK_H

#include <stddef.h>
#include <stdint.h>

struct network_sample {
    uint64_t rx_bytes;
    uint64_t tx_bytes;
};

int network_parse_line(const char *line,
                       char *name,
                       size_t name_cap,
                       uint64_t *rx_bytes,
                       uint64_t *tx_bytes);
int network_read(struct network_sample *out);
int network_rate(const struct network_sample *prev,
                 const struct network_sample *curr,
                 double seconds,
                 double *rx_per_sec,
                 double *tx_per_sec);

#endif

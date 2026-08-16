#include "diagnosis.h"

#include <stdio.h>
#include <string.h>

#define KIB_PER_GIB 1048576.0
#define KIB_PER_MIB 1024.0
#define BYTES_PER_GIB 1073741824.0

static int has_name(const char *name)
{
    return name != NULL && name[0] != '\0';
}

int diagnosis_build(const struct diagnosis_input *input, struct diagnosis *out)
{
    if (input == NULL || out == NULL) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    if (input->disk_percent >= 95.0) {
        out->focus = DIAGNOSIS_DISK;
        (void)snprintf(out->headline, sizeof(out->headline),
                       "Root storage is almost full.");
        (void)snprintf(out->detail, sizeof(out->detail),
                       "Only %.1f GiB is available. Running out of root space can break logs, updates, databases, and normal writes.",
                       (double)input->disk_available_bytes / BYTES_PER_GIB);
        return 0;
    }

    if (input->memory_percent >= 90.0) {
        out->focus = DIAGNOSIS_MEMORY;
        (void)snprintf(out->headline, sizeof(out->headline),
                       "Available memory is getting tight.");
        if (has_name(input->top_memory_name)) {
            (void)snprintf(out->detail, sizeof(out->detail),
                           "%.1f GiB is still available. %s is the largest visible process at %.1f MiB, but one process may not explain all memory use.",
                           (double)input->memory_available_kib / KIB_PER_GIB,
                           input->top_memory_name,
                           (double)input->top_memory_kib / KIB_PER_MIB);
        } else {
            (void)snprintf(out->detail, sizeof(out->detail),
                           "%.1f GiB is still available. Watch whether available memory keeps falling before treating this as sustained pressure.",
                           (double)input->memory_available_kib / KIB_PER_GIB);
        }
        return 0;
    }

    if (input->cpu_percent >= 90.0) {
        out->focus = DIAGNOSIS_CPU;
        (void)snprintf(out->headline, sizeof(out->headline),
                       "The CPUs are very busy right now.");
        if (has_name(input->top_cpu_name) && input->top_cpu_percent >= 15.0) {
            (void)snprintf(out->detail, sizeof(out->detail),
                           "%s accounts for about %.1f%% of whole-machine CPU in this short sample. A brief spike can still be normal.",
                           input->top_cpu_name,
                           input->top_cpu_percent);
        } else {
            (void)snprintf(out->detail, sizeof(out->detail),
                           "No single visible process dominates this short sample. Watch whether the high usage persists before drawing a conclusion.");
        }
        return 0;
    }

    if (input->load_ratio >= 1.25) {
        out->focus = DIAGNOSIS_LOAD;
        (void)snprintf(out->headline, sizeof(out->headline),
                       "More work is waiting than this CPU count handles comfortably.");
        if (input->cpu_percent < 70.0) {
            (void)snprintf(out->detail, sizeof(out->detail),
                           "CPU is not saturated, so some of the load may be waiting on I/O or other kernel work. Load is not another CPU percentage.");
        } else {
            (void)snprintf(out->detail, sizeof(out->detail),
                           "CPU is busy too, so runnable work may be queueing for processor time. Watch whether both values stay high.");
        }
        return 0;
    }

    out->focus = DIAGNOSIS_HEALTHY;
    (void)snprintf(out->headline, sizeof(out->headline),
                   "Nothing looks constrained right now.");
    (void)snprintf(out->detail, sizeof(out->detail),
                   "CPU, available memory, load, and root storage all have comfortable room. Short spikes can still happen and are usually normal.");
    return 0;
}

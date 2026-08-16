#ifndef ROOKIETOP_PROCESS_LIST_H
#define ROOKIETOP_PROCESS_LIST_H

#include <stddef.h>
#include <stdint.h>

#define PROCESS_LIST_NAME_MAX 64
#define PROCESS_CMDLINE_MAX 256

struct process_row {
    int pid;
    char name[PROCESS_LIST_NAME_MAX];
    char state;
    unsigned long threads;
    uint64_t rss_kib;
    uint64_t starttime;
    double cpu_percent;
};

struct process_detail {
    struct process_row row;
    char command[PROCESS_CMDLINE_MAX];
};

enum process_signal_result {
    PROCESS_SIGNAL_OK = 0,
    PROCESS_SIGNAL_GONE,
    PROCESS_SIGNAL_PERMISSION,
    PROCESS_SIGNAL_REUSED,
    PROCESS_SIGNAL_ERROR,
};

int process_list_read(struct process_row **out, size_t *out_count);
void process_list_free(struct process_row *rows);
int process_detail_read(int pid, struct process_detail *out);
int process_same_instance_alive(int pid, uint64_t starttime);
enum process_signal_result process_send_signal(int pid, uint64_t starttime, int signal_number);

#endif

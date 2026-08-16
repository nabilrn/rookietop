#include "process_list.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static int test_self_inspection(void)
{
    int self = (int)getpid();
    struct process_detail detail;
    if (process_detail_read(self, &detail) != 0 ||
        detail.row.pid != self || detail.row.name[0] == '\0' ||
        detail.row.starttime == 0 || detail.row.threads == 0) {
        return -1;
    }

    if (process_same_instance_alive(self, detail.row.starttime) != 1) {
        return -1;
    }

    struct process_row *rows = NULL;
    size_t count = 0;
    if (process_list_read(&rows, &count) != 0 || rows == NULL || count == 0) {
        process_list_free(rows);
        return -1;
    }

    int found_self = 0;
    for (size_t i = 0; i < count; i++) {
        if (rows[i].pid == self && rows[i].starttime == detail.row.starttime) {
            found_self = 1;
            break;
        }
    }
    process_list_free(rows);
    return found_self ? 0 : -1;
}

static int test_guarded_signal(void)
{
    pid_t child = fork();
    if (child < 0) {
        return -1;
    }
    if (child == 0) {
        for (;;) {
            pause();
        }
    }

    struct process_detail detail;
    if (process_detail_read((int)child, &detail) != 0) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, NULL, 0);
        return -1;
    }

    enum process_signal_result wrong = process_send_signal((int)child,
                                                            detail.row.starttime + 1,
                                                            SIGTERM);
    if (wrong != PROCESS_SIGNAL_REUSED || kill(child, 0) != 0) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, NULL, 0);
        return -1;
    }

    if (process_send_signal((int)child, detail.row.starttime, SIGTERM) != PROCESS_SIGNAL_OK) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, NULL, 0);
        return -1;
    }

    int status = 0;
    if (waitpid(child, &status, 0) != child || !WIFSIGNALED(status) || WTERMSIG(status) != SIGTERM) {
        return -1;
    }
    return 0;
}

int main(void)
{
    if (test_self_inspection() != 0 || test_guarded_signal() != 0) {
        return EXIT_FAILURE;
    }

    puts("process list tests: ok");
    return EXIT_SUCCESS;
}

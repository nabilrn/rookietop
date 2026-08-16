#include "process_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    int self = (int)getpid();
    struct process_detail detail;
    if (process_detail_read(self, &detail) != 0 ||
        detail.row.pid != self || detail.row.name[0] == '\0' ||
        detail.row.starttime == 0 || detail.row.threads == 0) {
        return EXIT_FAILURE;
    }

    if (process_same_instance_alive(self, detail.row.starttime) != 1) {
        return EXIT_FAILURE;
    }

    struct process_row *rows = NULL;
    size_t count = 0;
    if (process_list_read(&rows, &count) != 0 || rows == NULL || count == 0) {
        process_list_free(rows);
        return EXIT_FAILURE;
    }

    int found_self = 0;
    for (size_t i = 0; i < count; i++) {
        if (rows[i].pid == self && rows[i].starttime == detail.row.starttime) {
            found_self = 1;
            break;
        }
    }
    process_list_free(rows);

    if (!found_self) {
        return EXIT_FAILURE;
    }

    puts("process list tests: ok");
    return EXIT_SUCCESS;
}

#include "process_query.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    struct process_row rows[] = {
        {.pid = 101, .name = "nginx"},
        {.pid = 202, .name = "MariaDB"},
        {.pid = 303, .name = "cloudflared"},
    };

    if (!process_query_matches(&rows[0], "NGI") ||
        !process_query_matches(&rows[1], "maria") ||
        !process_query_matches(&rows[2], "03") ||
        process_query_matches(&rows[0], "postgres")) {
        return 1;
    }

    struct process_row filtered[3];
    memcpy(filtered, rows, sizeof(rows));
    size_t count = process_query_filter(filtered, 3, "a");
    if (count != 2 || strcmp(filtered[0].name, "MariaDB") != 0 ||
        strcmp(filtered[1].name, "cloudflared") != 0) {
        return 1;
    }

    memcpy(filtered, rows, sizeof(rows));
    count = process_query_filter(filtered, 3, "202");
    if (count != 1 || filtered[0].pid != 202) {
        return 1;
    }

    puts("process query tests passed");
    return 0;
}

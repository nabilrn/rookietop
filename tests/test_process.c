#include "process.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    FILE *file = fopen("tests/fixtures/proc_status_valid.txt", "r");
    if (file == NULL) {
        return 1;
    }

    struct process_info process;
    int result = process_parse_status(file, 4242, &process);
    fclose(file);

    if (result != 0 || process.pid != 4242 ||
        strcmp(process.name, "postgres worker") != 0 ||
        process.rss_kib != 262144) {
        return 1;
    }

    file = fopen("tests/fixtures/proc_status_missing_name.txt", "r");
    if (file == NULL) {
        return 1;
    }

    result = process_parse_status(file, 1, &process);
    fclose(file);

    if (result == 0) {
        return 1;
    }

    puts("process tests passed");
    return 0;
}

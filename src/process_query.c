#include "process_query.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int contains_case_insensitive(const char *text, const char *query)
{
    if (query[0] == '\0') {
        return 1;
    }

    for (const char *start = text; *start != '\0'; start++) {
        const char *left = start;
        const char *right = query;
        while (*left != '\0' && *right != '\0' &&
               tolower((unsigned char)*left) == tolower((unsigned char)*right)) {
            left++;
            right++;
        }
        if (*right == '\0') {
            return 1;
        }
    }
    return 0;
}

int process_query_matches(const struct process_row *row, const char *query)
{
    if (row == NULL || query == NULL) {
        return 0;
    }
    if (query[0] == '\0' || contains_case_insensitive(row->name, query)) {
        return 1;
    }

    char pid[32];
    int written = snprintf(pid, sizeof(pid), "%d", row->pid);
    return written > 0 && (size_t)written < sizeof(pid) && strstr(pid, query) != NULL;
}

size_t process_query_filter(struct process_row *rows, size_t count, const char *query)
{
    if (rows == NULL || query == NULL || query[0] == '\0') {
        return count;
    }

    size_t write = 0;
    for (size_t read = 0; read < count; read++) {
        if (process_query_matches(&rows[read], query)) {
            rows[write++] = rows[read];
        }
    }
    return write;
}

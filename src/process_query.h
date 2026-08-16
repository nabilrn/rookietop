#ifndef ROOKIETOP_PROCESS_QUERY_H
#define ROOKIETOP_PROCESS_QUERY_H

#include "process_list.h"

#include <stddef.h>

int process_query_matches(const struct process_row *row, const char *query);
size_t process_query_filter(struct process_row *rows, size_t count, const char *query);

#endif

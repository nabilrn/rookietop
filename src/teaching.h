#ifndef ROOKIETOP_TEACHING_H
#define ROOKIETOP_TEACHING_H

#include <stddef.h>

enum teaching_concept {
    TEACH_CPU = 0,
    TEACH_MEMORY,
    TEACH_LOAD,
    TEACH_PROCESS,
    TEACH_PID,
    TEACH_PROCESS_STATE,
    TEACH_SIGNALS,
    TEACH_DISK,
    TEACH_NETWORK,
    TEACH_COUNT,
};

struct teaching_topic {
    const char *title;
    const char *summary;
    const char *what;
    const char *why;
    const char *how;
    const char *try_it;
    const char *source;
};

const struct teaching_topic *teaching_get(enum teaching_concept concept);
const char *teaching_state_name(char state);
const char *teaching_state_explanation(char state);
size_t teaching_count(void);

#endif

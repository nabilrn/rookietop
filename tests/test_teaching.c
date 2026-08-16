#include "teaching.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    if (teaching_count() != TEACH_COUNT) {
        return EXIT_FAILURE;
    }

    for (int i = 0; i < TEACH_COUNT; i++) {
        const struct teaching_topic *topic = teaching_get((enum teaching_concept)i);
        if (topic == NULL || topic->title == NULL || topic->summary == NULL ||
            topic->what == NULL || topic->why == NULL || topic->how == NULL ||
            topic->try_it == NULL || topic->source == NULL ||
            topic->title[0] == '\0' || topic->what[0] == '\0' || topic->source[0] == '\0') {
            return EXIT_FAILURE;
        }
    }

    if (teaching_get((enum teaching_concept)-1) != NULL ||
        teaching_get((enum teaching_concept)TEACH_COUNT) != NULL) {
        return EXIT_FAILURE;
    }

    if (strcmp(teaching_state_name('S'), "Sleeping") != 0 ||
        strstr(teaching_state_explanation('S'), "usually healthy") == NULL ||
        strcmp(teaching_state_name('Z'), "Zombie") != 0) {
        return EXIT_FAILURE;
    }

    puts("teaching tests: ok");
    return EXIT_SUCCESS;
}

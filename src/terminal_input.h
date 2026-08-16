#ifndef ROOKIETOP_TERMINAL_INPUT_H
#define ROOKIETOP_TERMINAL_INPUT_H

#include <termios.h>

enum input_key {
    INPUT_NONE = 0,
    INPUT_UP = 1000,
    INPUT_DOWN,
    INPUT_ENTER,
    INPUT_ESCAPE,
};

struct terminal_input {
    struct termios saved;
    int active;
};

int terminal_input_enable(struct terminal_input *input);
void terminal_input_restore(struct terminal_input *input);
int terminal_input_read_key(int timeout_ms);

#endif

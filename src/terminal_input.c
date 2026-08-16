#define _POSIX_C_SOURCE 200809L

#include "terminal_input.h"

#include <errno.h>
#include <poll.h>
#include <unistd.h>

int terminal_input_enable(struct terminal_input *input)
{
    if (input == NULL || tcgetattr(STDIN_FILENO, &input->saved) != 0) {
        return -1;
    }

    struct termios raw = input->saved;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_iflag &= (tcflag_t)~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return -1;
    }

    input->active = 1;
    return 0;
}

void terminal_input_restore(struct terminal_input *input)
{
    if (input != NULL && input->active) {
        (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &input->saved);
        input->active = 0;
    }
}

static int read_byte(unsigned char *out)
{
    for (;;) {
        ssize_t count = read(STDIN_FILENO, out, 1);
        if (count == 1) {
            return 1;
        }
        if (count == 0) {
            return 0;
        }
        if (errno != EINTR) {
            return -1;
        }
    }
}

static int wait_input(int timeout_ms)
{
    struct pollfd fd = {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};

    for (;;) {
        int result = poll(&fd, 1, timeout_ms);
        if (result >= 0) {
            return result;
        }
        if (errno != EINTR) {
            return -1;
        }
    }
}

int terminal_input_read_key(int timeout_ms)
{
    int ready = wait_input(timeout_ms);
    if (ready <= 0) {
        return ready == 0 ? INPUT_NONE : -1;
    }

    unsigned char first;
    if (read_byte(&first) != 1) {
        return INPUT_NONE;
    }

    if (first == '\r' || first == '\n') {
        return INPUT_ENTER;
    }
    if (first != 0x1b) {
        return (int)first;
    }

    if (wait_input(8) <= 0) {
        return INPUT_ESCAPE;
    }

    unsigned char second;
    if (read_byte(&second) != 1 || second != '[' || wait_input(8) <= 0) {
        return INPUT_ESCAPE;
    }

    unsigned char third;
    if (read_byte(&third) != 1) {
        return INPUT_ESCAPE;
    }
    if (third == 'A') {
        return INPUT_UP;
    }
    if (third == 'B') {
        return INPUT_DOWN;
    }

    return INPUT_ESCAPE;
}

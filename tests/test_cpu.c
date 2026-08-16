#include "cpu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                           \
        }                                                                       \
    } while (0)

static int read_first_line(const char *path, char *buffer, size_t capacity)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    char *read = fgets(buffer, capacity, file);
    int close_result = fclose(file);
    return read != NULL && close_result == 0 ? 0 : -1;
}

static int test_parse_fixture(void)
{
    char line[256];
    struct cpu_sample sample;

    CHECK(read_first_line("tests/fixtures/proc_stat_valid.txt", line, sizeof(line)) == 0);
    CHECK(cpu_parse_line(line, &sample) == 0);
    CHECK(sample.user == 100);
    CHECK(sample.nice == 2);
    CHECK(sample.system == 30);
    CHECK(sample.idle == 400);
    CHECK(sample.iowait == 5);
    CHECK(sample.irq == 6);
    CHECK(sample.softirq == 7);
    CHECK(sample.steal == 8);
    return 0;
}

static int test_parse_minimum_line(void)
{
    struct cpu_sample sample;

    CHECK(cpu_parse_line("cpu 1 2 3 4\n", &sample) == 0);
    CHECK(sample.user == 1);
    CHECK(sample.idle == 4);
    CHECK(sample.iowait == 0);
    CHECK(sample.steal == 0);
    return 0;
}

static int test_parse_rejects_malformed_input(void)
{
    char line[256];
    struct cpu_sample sample;

    CHECK(read_first_line("tests/fixtures/proc_stat_malformed.txt", line, sizeof(line)) == 0);
    CHECK(cpu_parse_line(line, &sample) != 0);
    CHECK(cpu_parse_line("intr 1 2 3 4\n", &sample) != 0);
    CHECK(cpu_parse_line(NULL, &sample) != 0);
    CHECK(cpu_parse_line("cpu 1 2 3 4\n", NULL) != 0);
    return 0;
}

static int test_usage(void)
{
    struct cpu_sample prev = {.user = 100, .system = 50, .idle = 850};
    struct cpu_sample curr = {.user = 160, .system = 70, .idle = 870};
    double usage;

    CHECK(cpu_usage(&prev, &curr, &usage) == 0);
    CHECK(fabs(usage - 80.0) < 0.0001);
    return 0;
}

static int test_usage_includes_iowait_as_inactive(void)
{
    struct cpu_sample prev = {.user = 10, .idle = 80, .iowait = 10};
    struct cpu_sample curr = {.user = 20, .idle = 85, .iowait = 15};
    double usage;

    CHECK(cpu_usage(&prev, &curr, &usage) == 0);
    CHECK(fabs(usage - 50.0) < 0.0001);
    return 0;
}

static int test_usage_rejects_zero_delta(void)
{
    struct cpu_sample sample = {.user = 1, .idle = 1};
    double usage;

    CHECK(cpu_usage(&sample, &sample, &usage) != 0);
    return 0;
}

static int test_usage_rejects_counter_regression(void)
{
    struct cpu_sample prev = {.user = 10, .idle = 20};
    struct cpu_sample curr = {.user = 9, .idle = 21};
    double usage;

    CHECK(cpu_usage(&prev, &curr, &usage) != 0);
    return 0;
}

int main(void)
{
    if (test_parse_fixture() != 0 ||
        test_parse_minimum_line() != 0 ||
        test_parse_rejects_malformed_input() != 0 ||
        test_usage() != 0 ||
        test_usage_includes_iowait_as_inactive() != 0 ||
        test_usage_rejects_zero_delta() != 0 ||
        test_usage_rejects_counter_regression() != 0) {
        return EXIT_FAILURE;
    }

    puts("cpu tests: ok");
    return EXIT_SUCCESS;
}

#include "memory.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void test_valid_fixture(void)
{
    FILE *file = fopen("tests/fixtures/proc_meminfo_valid.txt", "r");
    require(file != NULL, "open valid fixture");

    struct memory_sample sample;
    require(memory_parse(file, &sample) == 0, "parse valid meminfo");
    require(fclose(file) == 0, "close valid fixture");
    require(sample.total_kib == 8192000, "MemTotal parsed");
    require(sample.available_kib == 2048000, "MemAvailable parsed");
    require(sample.swap_total_kib == 2097152, "SwapTotal parsed");
    require(sample.swap_free_kib == 1572864, "SwapFree parsed");
}

static void test_usage(void)
{
    struct memory_sample sample = {
        .total_kib = 8000,
        .available_kib = 2000,
        .swap_total_kib = 1000,
        .swap_free_kib = 750,
    };
    double percent;
    uint64_t used;
    uint64_t swap_used;

    require(memory_usage(&sample, &percent, &used, &swap_used) == 0,
            "calculate memory usage");
    require(fabs(percent - 75.0) < 0.0001, "usage is 75 percent");
    require(used == 6000, "used derives from MemAvailable");
    require(swap_used == 250, "swap used calculation");
}

static void test_missing_available(void)
{
    FILE *file = fopen("tests/fixtures/proc_meminfo_missing_available.txt", "r");
    require(file != NULL, "open missing available fixture");

    struct memory_sample sample;
    require(memory_parse(file, &sample) != 0, "reject missing MemAvailable");
    require(fclose(file) == 0, "close missing available fixture");
}

static void test_bad_unit(void)
{
    FILE *file = fopen("tests/fixtures/proc_meminfo_bad_unit.txt", "r");
    require(file != NULL, "open bad unit fixture");

    struct memory_sample sample;
    require(memory_parse(file, &sample) != 0, "reject unexpected unit");
    require(fclose(file) == 0, "close bad unit fixture");
}

static void test_invalid_ranges(void)
{
    struct memory_sample sample = {
        .total_kib = 1000,
        .available_kib = 1001,
        .swap_total_kib = 0,
        .swap_free_kib = 0,
    };
    double percent;
    uint64_t used;
    uint64_t swap_used;

    require(memory_usage(&sample, &percent, &used, &swap_used) != 0,
            "reject available greater than total");
}

int main(void)
{
    test_valid_fixture();
    test_usage();
    test_missing_available();
    test_bad_unit();
    test_invalid_ranges();
    puts("memory tests passed");
    return 0;
}

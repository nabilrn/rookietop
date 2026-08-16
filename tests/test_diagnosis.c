#include "diagnosis.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct diagnosis_input base_input(void)
{
    struct diagnosis_input input = {
        .cpu_percent = 20.0,
        .memory_percent = 40.0,
        .disk_percent = 30.0,
        .load_ratio = 0.2,
        .memory_available_kib = 2ULL * 1024ULL * 1024ULL,
        .disk_available_bytes = 40ULL * 1024ULL * 1024ULL * 1024ULL,
        .top_cpu_name = "worker",
        .top_cpu_percent = 10.0,
        .top_memory_name = "database",
        .top_memory_kib = 256ULL * 1024ULL,
    };
    return input;
}

int main(void)
{
    struct diagnosis_input input = base_input();
    struct diagnosis out;

    assert(diagnosis_build(&input, &out) == 0);
    assert(out.focus == DIAGNOSIS_HEALTHY);
    assert(strstr(out.headline, "Nothing") != NULL);

    input = base_input();
    input.cpu_percent = 96.0;
    input.top_cpu_name = "gcc";
    input.top_cpu_percent = 64.0;
    assert(diagnosis_build(&input, &out) == 0);
    assert(out.focus == DIAGNOSIS_CPU);
    assert(strstr(out.detail, "gcc") != NULL);

    input = base_input();
    input.memory_percent = 94.0;
    input.memory_available_kib = 512ULL * 1024ULL;
    assert(diagnosis_build(&input, &out) == 0);
    assert(out.focus == DIAGNOSIS_MEMORY);
    assert(strstr(out.detail, "database") != NULL);

    input = base_input();
    input.disk_percent = 97.0;
    assert(diagnosis_build(&input, &out) == 0);
    assert(out.focus == DIAGNOSIS_DISK);

    input = base_input();
    input.load_ratio = 1.5;
    input.cpu_percent = 35.0;
    assert(diagnosis_build(&input, &out) == 0);
    assert(out.focus == DIAGNOSIS_LOAD);
    assert(strstr(out.detail, "I/O") != NULL);

    assert(diagnosis_build(NULL, &out) == -1);
    assert(diagnosis_build(&input, NULL) == -1);

    puts("diagnosis tests passed");
    return 0;
}

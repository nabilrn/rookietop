#define _POSIX_C_SOURCE 200809L

#include "host.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#define PROC_UPTIME_PATH "/proc/uptime"
#define PROC_LOADAVG_PATH "/proc/loadavg"
#define THERMAL_ROOT "/sys/class/thermal"
#define THERMAL_PATH_MAX 256
#define THERMAL_LINE_MAX 128

int host_parse_uptime(FILE *file, double *out_seconds)
{
    if (file == NULL || out_seconds == NULL) {
        return -1;
    }

    double uptime;
    if (fscanf(file, "%lf", &uptime) != 1 || uptime < 0.0) {
        return -1;
    }

    *out_seconds = uptime;
    return 0;
}

int host_parse_loadavg(FILE *file, double *out_load1, double *out_load5, double *out_load15)
{
    if (file == NULL || out_load1 == NULL || out_load5 == NULL || out_load15 == NULL) {
        return -1;
    }

    double load1;
    double load5;
    double load15;
    if (fscanf(file, "%lf %lf %lf", &load1, &load5, &load15) != 3 ||
        load1 < 0.0 || load5 < 0.0 || load15 < 0.0) {
        return -1;
    }

    *out_load1 = load1;
    *out_load5 = load5;
    *out_load15 = load15;
    return 0;
}

static int read_first_line(const char *path, char *out, size_t cap)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    char *read = fgets(out, (int)cap, file);
    int close_result = fclose(file);
    if (read == NULL || close_result != 0) {
        return -1;
    }

    out[strcspn(out, "\r\n")] = '\0';
    return out[0] == '\0' ? -1 : 0;
}

static void read_thermal(struct host_info *out)
{
    DIR *dir = opendir(THERMAL_ROOT);
    if (dir == NULL) {
        return;
    }

    double hottest = -1.0;
    char hottest_type[HOST_THERMAL_TYPE_MAX] = {0};

    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (entry == NULL) {
            break;
        }

        if (strncmp(entry->d_name, "thermal_zone", 12) != 0) {
            continue;
        }

        char temp_path[THERMAL_PATH_MAX];
        char type_path[THERMAL_PATH_MAX];
        int temp_written = snprintf(temp_path,
                                    sizeof(temp_path),
                                    THERMAL_ROOT "/%s/temp",
                                    entry->d_name);
        int type_written = snprintf(type_path,
                                    sizeof(type_path),
                                    THERMAL_ROOT "/%s/type",
                                    entry->d_name);
        if (temp_written < 0 || type_written < 0 ||
            (size_t)temp_written >= sizeof(temp_path) ||
            (size_t)type_written >= sizeof(type_path)) {
            continue;
        }

        char temp_line[THERMAL_LINE_MAX];
        if (read_first_line(temp_path, temp_line, sizeof(temp_line)) != 0) {
            continue;
        }

        errno = 0;
        char *end;
        long millidegrees = strtol(temp_line, &end, 10);
        if (errno != 0 || *end != '\0' || millidegrees < 0 || millidegrees > 200000) {
            continue;
        }

        double temperature = (double)millidegrees / 1000.0;
        if (temperature <= hottest) {
            continue;
        }

        char type[HOST_THERMAL_TYPE_MAX] = "thermal";
        (void)read_first_line(type_path, type, sizeof(type));
        hottest = temperature;
        memcpy(hottest_type, type, sizeof(hottest_type));
        hottest_type[sizeof(hottest_type) - 1] = '\0';
    }

    (void)closedir(dir);

    if (hottest >= 0.0) {
        out->thermal_ok = 1;
        out->thermal_c = hottest;
        memcpy(out->thermal_type, hottest_type, sizeof(out->thermal_type));
        out->thermal_type[sizeof(out->thermal_type) - 1] = '\0';
    }
}

int host_read(struct host_info *out)
{
    if (out == NULL) {
        return -1;
    }

    struct host_info info = {0};
    struct utsname uts;

    if (gethostname(info.hostname, sizeof(info.hostname)) != 0) {
        return -1;
    }
    info.hostname[sizeof(info.hostname) - 1] = '\0';

    if (uname(&uts) != 0) {
        return -1;
    }
    if (snprintf(info.kernel_release, sizeof(info.kernel_release), "%s", uts.release) < 0) {
        return -1;
    }

    info.cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (info.cores < 1) {
        return -1;
    }

    FILE *file = fopen(PROC_UPTIME_PATH, "r");
    if (file == NULL) {
        return -1;
    }
    int result = host_parse_uptime(file, &info.uptime_seconds);
    int close_result = fclose(file);
    if (result != 0 || close_result != 0) {
        return -1;
    }

    file = fopen(PROC_LOADAVG_PATH, "r");
    if (file == NULL) {
        return -1;
    }
    result = host_parse_loadavg(file, &info.load1, &info.load5, &info.load15);
    close_result = fclose(file);
    if (result != 0 || close_result != 0) {
        return -1;
    }

    read_thermal(&info);
    *out = info;
    return 0;
}

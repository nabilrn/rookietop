#include <stdio.h>
#include <string.h>

#define ROOKIETOP_VERSION "0.0.1-dev"

static void print_help(void)
{
    puts("RookieTop - beginner-first Linux system monitor");
    puts("");
    puts("Usage: rookietop [OPTION]");
    puts("");
    puts("Options:");
    puts("  -h, --help       Show this help");
    puts("  -V, --version    Show version");
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        puts("RookieTop bootstrap is ready.");
        puts("Collectors are intentionally not implemented yet.");
        return 0;
    }

    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_help();
        return 0;
    }

    if (argc == 2 && (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0)) {
        puts(ROOKIETOP_VERSION);
        return 0;
    }

    fprintf(stderr, "rookietop: unknown option\n");
    return 2;
}

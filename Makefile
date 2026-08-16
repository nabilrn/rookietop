CC ?= cc
CPPFLAGS ?= -Isrc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDFLAGS ?=
LDLIBS ?=

TARGET := rookietop
SRC := src/main.c src/cpu.c src/memory.c
CPU_TEST := tests/test_cpu
CPU_TEST_SRC := tests/test_cpu.c src/cpu.c
MEMORY_TEST := tests/test_memory
MEMORY_TEST_SRC := tests/test_memory.c src/memory.c

.PHONY: all clean check debug test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $@

$(CPU_TEST): $(CPU_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CPU_TEST_SRC) $(LDFLAGS) -lm -o $@

$(MEMORY_TEST): $(MEMORY_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MEMORY_TEST_SRC) $(LDFLAGS) -lm -o $@

test: $(CPU_TEST) $(MEMORY_TEST)
	./$(CPU_TEST)
	./$(MEMORY_TEST)

debug:
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(SRC) -fsanitize=address,undefined -o $(TARGET)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(CPU_TEST_SRC) -fsanitize=address,undefined -lm -o $(CPU_TEST)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(MEMORY_TEST_SRC) -fsanitize=address,undefined -lm -o $(MEMORY_TEST)

check: $(TARGET) test
	./$(TARGET) --help >/dev/null
	./$(TARGET) --version >/dev/null
	./$(TARGET) >/dev/null

clean:
	rm -f $(TARGET) $(CPU_TEST) $(MEMORY_TEST)

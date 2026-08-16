CC ?= cc
CPPFLAGS ?= -Isrc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDFLAGS ?=
LDLIBS ?=

TARGET := rookietop
SRC := src/main.c src/cpu.c
TEST_TARGET := tests/test_cpu
TEST_SRC := tests/test_cpu.c src/cpu.c

.PHONY: all clean check debug test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_TARGET): $(TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_SRC) $(LDFLAGS) -lm -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

debug:
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(SRC) -fsanitize=address,undefined -o $(TARGET)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(TEST_SRC) -fsanitize=address,undefined -lm -o $(TEST_TARGET)

check: $(TARGET) test
	./$(TARGET) --help >/dev/null
	./$(TARGET) --version >/dev/null
	./$(TARGET) >/dev/null

clean:
	rm -f $(TARGET) $(TEST_TARGET)

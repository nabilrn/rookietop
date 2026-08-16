CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDFLAGS ?=
LDLIBS ?=

TARGET := rookietop
SRC := src/main.c

.PHONY: all clean check debug

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $@

debug:
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(SRC) -fsanitize=address,undefined -o $(TARGET)

check: $(TARGET)
	./$(TARGET) --help >/dev/null
	./$(TARGET) --version >/dev/null

clean:
	rm -f $(TARGET)

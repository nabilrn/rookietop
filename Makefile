CC ?= cc
CPPFLAGS ?= -Isrc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDFLAGS ?=
LDLIBS ?=

TARGET := rookietop
SRC := src/main.c src/cpu.c src/memory.c src/disk.c src/network.c src/process.c src/process_cpu.c src/process_list.c src/process_query.c src/teaching.c src/diagnosis.c src/terminal_input.c src/host.c
CPU_TEST := tests/test_cpu
CPU_TEST_SRC := tests/test_cpu.c src/cpu.c
MEMORY_TEST := tests/test_memory
MEMORY_TEST_SRC := tests/test_memory.c src/memory.c
DISK_TEST := tests/test_disk
DISK_TEST_SRC := tests/test_disk.c src/disk.c
NETWORK_TEST := tests/test_network
NETWORK_TEST_SRC := tests/test_network.c src/network.c
PROCESS_TEST := tests/test_process
PROCESS_TEST_SRC := tests/test_process.c src/process.c
PROCESS_CPU_TEST := tests/test_process_cpu
PROCESS_CPU_TEST_SRC := tests/test_process_cpu.c src/process_cpu.c
PROCESS_LIST_TEST := tests/test_process_list
PROCESS_LIST_TEST_SRC := tests/test_process_list.c src/process_list.c src/process_cpu.c
PROCESS_QUERY_TEST := tests/test_process_query
PROCESS_QUERY_TEST_SRC := tests/test_process_query.c src/process_query.c
TEACHING_TEST := tests/test_teaching
TEACHING_TEST_SRC := tests/test_teaching.c src/teaching.c
DIAGNOSIS_TEST := tests/test_diagnosis
DIAGNOSIS_TEST_SRC := tests/test_diagnosis.c src/diagnosis.c
HOST_TEST := tests/test_host
HOST_TEST_SRC := tests/test_host.c src/host.c

.PHONY: all clean check debug test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $@

$(CPU_TEST): $(CPU_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CPU_TEST_SRC) $(LDFLAGS) -lm -o $@

$(MEMORY_TEST): $(MEMORY_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MEMORY_TEST_SRC) $(LDFLAGS) -lm -o $@

$(DISK_TEST): $(DISK_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DISK_TEST_SRC) $(LDFLAGS) -lm -o $@

$(NETWORK_TEST): $(NETWORK_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NETWORK_TEST_SRC) $(LDFLAGS) -lm -o $@

$(PROCESS_TEST): $(PROCESS_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PROCESS_TEST_SRC) $(LDFLAGS) -o $@

$(PROCESS_CPU_TEST): $(PROCESS_CPU_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PROCESS_CPU_TEST_SRC) $(LDFLAGS) -o $@

$(PROCESS_LIST_TEST): $(PROCESS_LIST_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PROCESS_LIST_TEST_SRC) $(LDFLAGS) -o $@

$(PROCESS_QUERY_TEST): $(PROCESS_QUERY_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PROCESS_QUERY_TEST_SRC) $(LDFLAGS) -o $@

$(TEACHING_TEST): $(TEACHING_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEACHING_TEST_SRC) $(LDFLAGS) -o $@

$(DIAGNOSIS_TEST): $(DIAGNOSIS_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DIAGNOSIS_TEST_SRC) $(LDFLAGS) -o $@

$(HOST_TEST): $(HOST_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(HOST_TEST_SRC) $(LDFLAGS) -lm -o $@

test: $(CPU_TEST) $(MEMORY_TEST) $(DISK_TEST) $(NETWORK_TEST) $(PROCESS_TEST) $(PROCESS_CPU_TEST) $(PROCESS_LIST_TEST) $(PROCESS_QUERY_TEST) $(TEACHING_TEST) $(DIAGNOSIS_TEST) $(HOST_TEST)
	./$(CPU_TEST)
	./$(MEMORY_TEST)
	./$(DISK_TEST)
	./$(NETWORK_TEST)
	./$(PROCESS_TEST)
	./$(PROCESS_CPU_TEST)
	./$(PROCESS_LIST_TEST)
	./$(PROCESS_QUERY_TEST)
	./$(TEACHING_TEST)
	./$(DIAGNOSIS_TEST)
	./$(HOST_TEST)

debug:
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(SRC) -fsanitize=address,undefined -o $(TARGET)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(CPU_TEST_SRC) -fsanitize=address,undefined -lm -o $(CPU_TEST)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(MEMORY_TEST_SRC) -fsanitize=address,undefined -lm -o $(MEMORY_TEST)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(DISK_TEST_SRC) -fsanitize=address,undefined -lm -o $(DISK_TEST)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(NETWORK_TEST_SRC) -fsanitize=address,undefined -lm -o $(NETWORK_TEST)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(PROCESS_TEST_SRC) -fsanitize=address,undefined -o $(PROCESS_TEST)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(PROCESS_CPU_TEST_SRC) -fsanitize=address,undefined -o $(PROCESS_CPU_TEST)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(PROCESS_LIST_TEST_SRC) -fsanitize=address,undefined -o $(PROCESS_LIST_TEST)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(PROCESS_QUERY_TEST_SRC) -fsanitize=address,undefined -o $(PROCESS_QUERY_TEST)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(TEACHING_TEST_SRC) -fsanitize=address,undefined -o $(TEACHING_TEST)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(DIAGNOSIS_TEST_SRC) -fsanitize=address,undefined -o $(DIAGNOSIS_TEST)
	$(CC) $(CPPFLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined $(HOST_TEST_SRC) -fsanitize=address,undefined -lm -o $(HOST_TEST)

check: $(TARGET) test
	./$(TARGET) --help >/dev/null
	./$(TARGET) --version >/dev/null
	./$(TARGET) --once >/dev/null

clean:
	rm -f $(TARGET) $(CPU_TEST) $(MEMORY_TEST) $(DISK_TEST) $(NETWORK_TEST) $(PROCESS_TEST) $(PROCESS_CPU_TEST) $(PROCESS_LIST_TEST) $(PROCESS_QUERY_TEST) $(TEACHING_TEST) $(DIAGNOSIS_TEST) $(HOST_TEST)

#include "teaching.h"

static const struct teaching_topic topics[TEACH_COUNT] = {
    [TEACH_CPU] = {
        .title = "CPU usage",
        .summary = "How busy your processors are during a sampling window.",
        .what = "CPU usage estimates how much processor time was spent doing work instead of being idle.",
        .why = "A short spike is usually normal. Sustained high usage matters when work stays queued or the machine feels slow.",
        .how = "Linux exposes cumulative CPU-time counters. RookieTop reads two samples and calculates the change between them; the kernel does not store one ready-made CPU percentage.",
        .try_it = "Run `yes > /dev/null` in another terminal, watch CPU rise, then stop it with Ctrl+C.",
        .source = "/proc/stat",
    },
    [TEACH_MEMORY] = {
        .title = "Linux memory",
        .summary = "Why used RAM is not the same thing as memory pressure.",
        .what = "RAM holds running programs and data that Linux wants to access quickly.",
        .why = "Linux intentionally uses spare RAM for cache. High used memory alone is not a problem; low available memory is the more useful warning sign.",
        .how = "RookieTop uses MemAvailable rather than treating MemFree as all usable memory, so reclaimable filesystem cache is not counted as wasted RAM.",
        .try_it = "Compare RookieTop with `grep -E 'MemTotal|MemAvailable|MemFree|Cached' /proc/meminfo`.",
        .source = "/proc/meminfo",
    },
    [TEACH_LOAD] = {
        .title = "Load average",
        .summary = "Queue pressure, not another CPU percentage.",
        .what = "Load average describes runnable tasks plus tasks waiting in uninterruptible sleep, averaged over time.",
        .why = "A load of 2 can be light on a 16-CPU machine but meaningful on a 2-CPU machine. Context matters.",
        .how = "RookieTop compares the one-minute load value with the number of online CPUs instead of presenting load as a percentage.",
        .try_it = "Run `cat /proc/loadavg` and `nproc`, then compare both values with RookieTop.",
        .source = "/proc/loadavg + sysconf(_SC_NPROCESSORS_ONLN)",
    },
    [TEACH_PROCESS] = {
        .title = "Processes",
        .summary = "A running instance of a program that Linux tracks separately.",
        .what = "A process is a running program instance with its own identity, memory, execution state, threads, and resources.",
        .why = "When a machine is slow, processes are often where you find the actual consumer behind a CPU or memory number.",
        .how = "Linux exposes one procfs directory per process. RookieTop reads only the fields it needs instead of calling `ps`.",
        .try_it = "Run `sleep 300 &`, note the PID, then find that same process in RookieTop.",
        .source = "/proc/<pid>/status + /proc/<pid>/stat + /proc/<pid>/cmdline",
    },
    [TEACH_PID] = {
        .title = "PID",
        .summary = "The numeric identifier Linux assigns to a process.",
        .what = "PID means Process ID. Linux uses it to address one process at a particular moment.",
        .why = "PIDs are reused after processes exit, so a number by itself is not a permanent identity.",
        .how = "Before signalling a process, RookieTop checks both PID and process start time. If the PID was reused, the action is refused.",
        .try_it = "Run `echo $$` in your shell, then inspect `/proc/$$/status`.",
        .source = "/proc/<pid>/stat field 22 (starttime)",
    },
    [TEACH_PROCESS_STATE] = {
        .title = "Process state",
        .summary = "Why Sleeping usually means waiting normally, not broken.",
        .what = "Linux records a compact state for each process, such as running, sleeping, waiting on I/O, stopped, or zombie.",
        .why = "Most healthy processes spend much of their time sleeping because they are waiting for input, timers, network traffic, or other work.",
        .how = "RookieTop reads the state character from procfs and expands it into beginner-readable text.",
        .try_it = "Inspect a long-running service and a busy command such as `yes`; compare their states over time.",
        .source = "/proc/<pid>/stat + /proc/<pid>/status",
    },
    [TEACH_SIGNALS] = {
        .title = "Signals: TERM vs KILL",
        .summary = "How Linux asks a process to stop, and how force-stop differs.",
        .what = "A signal is a small asynchronous notification sent to a process. SIGTERM asks for a graceful shutdown; SIGKILL makes the kernel terminate it immediately.",
        .why = "Graceful shutdown gives software a chance to close files, flush data, and clean up. SIGKILL skips that opportunity.",
        .how = "RookieTop calls kill(2) directly. `k` sends SIGTERM; uppercase `K` requires a separate confirmation before SIGKILL.",
        .try_it = "Start `sleep 300`, find it in Process Explorer, and use Safe Stop. Observe that SIGTERM is enough.",
        .source = "kill(2), SIGTERM, SIGKILL",
    },
    [TEACH_DISK] = {
        .title = "Filesystem space",
        .summary = "How full the root filesystem is and why free space matters.",
        .what = "Filesystem usage tells you how much storage capacity is currently unavailable for new files.",
        .why = "A nearly full root filesystem can break updates, logs, databases, builds, and normal application writes.",
        .how = "RookieTop asks the kernel for root-filesystem capacity and available blocks rather than parsing the output of `df`.",
        .try_it = "Run `df -h /` and compare its root capacity with RookieTop.",
        .source = "statvfs(\"/\")",
    },
    [TEACH_NETWORK] = {
        .title = "Network throughput",
        .summary = "Traffic rate calculated from byte counters over time.",
        .what = "Network throughput shows how quickly bytes are entering and leaving the machine.",
        .why = "A transfer can explain bandwidth use, slow links, or why a server is busy even when CPU usage is modest.",
        .how = "Linux exposes cumulative RX/TX counters. RookieTop samples them twice and divides the byte difference by elapsed monotonic time.",
        .try_it = "Download a normal file or run a package update in another terminal and watch the receive rate rise.",
        .source = "/proc/net/dev + CLOCK_MONOTONIC",
    },
};

const struct teaching_topic *teaching_get(enum teaching_concept concept)
{
    if (concept < 0 || concept >= TEACH_COUNT) {
        return 0;
    }
    return &topics[concept];
}

const char *teaching_state_name(char state)
{
    switch (state) {
    case 'R': return "Running";
    case 'S': return "Sleeping";
    case 'D': return "Waiting I/O";
    case 'T':
    case 't': return "Stopped";
    case 'Z': return "Zombie";
    case 'I': return "Idle";
    default: return "Other";
    }
}

const char *teaching_state_explanation(char state)
{
    switch (state) {
    case 'R': return "The process is runnable and may be executing on a CPU right now.";
    case 'S': return "The process is waiting normally for work, input, a timer, or another event. Sleeping is usually healthy.";
    case 'D': return "The process is waiting in uninterruptible sleep, commonly for kernel or I/O work to finish.";
    case 'T':
    case 't': return "The process has been stopped or traced and is not currently running normal work.";
    case 'Z': return "The process exited, but its parent has not collected the exit status yet.";
    case 'I': return "The kernel reports this task as idle.";
    default: return "Linux exposes a process-state code that RookieTop does not yet describe in more detail.";
    }
}

size_t teaching_count(void)
{
    return TEACH_COUNT;
}

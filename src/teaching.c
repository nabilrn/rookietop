#include "teaching.h"

static const struct teaching_topic topics[TEACH_COUNT] = {
    [TEACH_CPU] = {
        .title = "CPU usage",
        .summary = "How busy your processors are right now.",
        .what = "CPU usage shows how busy your processors were during a short slice of time.",
        .why = "Brief spikes are normal. It becomes interesting when CPU stays busy long enough that work starts waiting or the machine feels slow.",
        .how = "Linux does not store a ready-made value such as 'CPU = 42%'. It keeps cumulative CPU-time counters, and RookieTop compares two samples to calculate the percentage.",
        .try_it = "Run `yes > /dev/null` in another terminal. Watch CPU rise, then stop the command with Ctrl+C.",
        .source = "/proc/stat",
    },
    [TEACH_MEMORY] = {
        .title = "Linux memory",
        .summary = "Why a lot of used RAM is not automatically bad.",
        .what = "RAM keeps programs and useful data close to the CPU so Linux can reach them quickly.",
        .why = "Linux also uses spare RAM as cache. That is useful, not wasted. The more important question is how much memory is still readily available when programs need it.",
        .how = "RookieTop reads MemAvailable instead of treating MemFree as all usable memory. This keeps reclaimable cache from looking like a memory problem.",
        .try_it = "Run `grep -E 'MemTotal|MemAvailable|MemFree|Cached' /proc/meminfo` and compare the values with RookieTop.",
        .source = "/proc/meminfo",
    },
    [TEACH_LOAD] = {
        .title = "Load average",
        .summary = "How much work is ready or waiting, not another CPU percent.",
        .what = "Load average is a clue about how much work is ready to run or stuck waiting in certain kernel states over time.",
        .why = "The same load number means different things on different machines. A load of 2 is easy for many CPUs but can be significant on a 2-CPU VM.",
        .how = "RookieTop reads Linux load values and compares the one-minute value with your number of online CPUs instead of converting load into a fake percentage.",
        .try_it = "Run `cat /proc/loadavg` and `nproc`, then compare both with the values shown by RookieTop.",
        .source = "/proc/loadavg + sysconf(_SC_NPROCESSORS_ONLN)",
    },
    [TEACH_PROCESS] = {
        .title = "Processes",
        .summary = "The running programs and background work behind system activity.",
        .what = "A process is one running instance of a program. Linux tracks its identity, memory, state, threads, and other resources separately.",
        .why = "When CPU or memory looks unusual, processes help answer the next question: which running program is actually contributing to it?",
        .how = "Linux creates a directory under /proc for each process. RookieTop reads the few fields it needs directly instead of launching `ps` behind the scenes.",
        .try_it = "Run `sleep 300 &`, note the PID printed by your shell, then find that same process in RookieTop.",
        .source = "/proc/<pid>/status + /proc/<pid>/stat + /proc/<pid>/cmdline",
    },
    [TEACH_PID] = {
        .title = "PID",
        .summary = "The number Linux is using for one running process right now.",
        .what = "PID means Process ID. It is the number Linux uses to refer to a running process at a particular moment.",
        .why = "A PID is not permanent. Linux can reuse the same number after a process exits, so safe process actions need more than the number alone.",
        .how = "Before RookieTop sends a signal, it checks both PID and process start time. If that identity changed, RookieTop refuses the action.",
        .try_it = "Run `echo $$` to see your shell's PID, then inspect `/proc/$$/status`.",
        .source = "/proc/<pid>/stat field 22 (starttime)",
    },
    [TEACH_PROCESS_STATE] = {
        .title = "Process state",
        .summary = "Why Sleeping is normal for most background processes.",
        .what = "Linux gives each process a short state such as running, sleeping, waiting on I/O, stopped, or zombie.",
        .why = "Most healthy services spend a lot of time sleeping because they have nothing to do until input, a timer, network traffic, or another event arrives.",
        .how = "RookieTop reads Linux's state code from procfs and turns common codes into plain-language labels.",
        .try_it = "Watch a quiet service and a busy command such as `yes`. Their states can change as their work changes.",
        .source = "/proc/<pid>/stat + /proc/<pid>/status",
    },
    [TEACH_SIGNALS] = {
        .title = "Stopping a process",
        .summary = "The difference between asking a process to exit and killing it immediately.",
        .what = "Linux signals are small messages sent to processes. SIGTERM asks a process to exit; SIGKILL tells the kernel to end it immediately.",
        .why = "A normal stop gives software a chance to finish work and clean up. A force kill skips that chance, so it should be a fallback rather than the first move.",
        .how = "RookieTop calls kill(2) directly. `k` sends SIGTERM after confirmation. Uppercase `K` is a separate, more dangerous SIGKILL action.",
        .try_it = "Start `sleep 300`, find it in Process Explorer, and use Safe Stop. SIGTERM should be enough.",
        .source = "kill(2), SIGTERM, SIGKILL",
    },
    [TEACH_DISK] = {
        .title = "Filesystem space",
        .summary = "How much room remains on the root filesystem.",
        .what = "Filesystem usage shows how much storage is occupied and how much room remains for new files.",
        .why = "Running out of root space can cause surprisingly unrelated failures: logs stop writing, updates fail, databases cannot grow, and applications cannot save files.",
        .how = "RookieTop asks the operating system for root-filesystem capacity directly instead of running and parsing `df`.",
        .try_it = "Run `df -h /` and compare the root filesystem values with RookieTop.",
        .source = "statvfs(\"/\")",
    },
    [TEACH_NETWORK] = {
        .title = "Network throughput",
        .summary = "How quickly data is entering and leaving this machine.",
        .what = "Network throughput shows the current receive and transmit rate for traffic RookieTop can observe on the machine.",
        .why = "A download, backup, package update, or busy service can explain network activity even when CPU use is modest.",
        .how = "Linux keeps cumulative receive and transmit byte counters. RookieTop compares two samples and divides the byte change by elapsed time.",
        .try_it = "Download a normal file or run a package update in another terminal and watch the receive rate change.",
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
    case 'R': return "Ready to run or using CPU right now.";
    case 'S': return "Waiting for work or an event. This is normal for many background processes.";
    case 'D': return "Waiting on kernel or I/O work that cannot be interrupted yet.";
    case 'T':
    case 't': return "Paused or being traced, so normal work is not running right now.";
    case 'Z': return "Already exited, but its parent has not collected the exit result yet.";
    case 'I': return "An idle kernel task with no work to run right now.";
    default: return "Linux reports a state RookieTop does not explain yet.";
    }
}

size_t teaching_count(void)
{
    return TEACH_COUNT;
}

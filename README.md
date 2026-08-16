# RookieTop

**Learn Linux by watching your own system work.**

RookieTop is a beginner-first Linux system monitor and interactive teaching tool written in C. It uses the machine you are currently running as the lesson instead of separating monitoring from learning.

RookieTop aims to answer four questions:

1. **What am I looking at?**
2. **Why does it matter?**
3. **How does Linux expose it?**
4. **What can I try myself?**

It reads Linux interfaces such as `/proc`, `/sys`, `statvfs()`, and `kill(2)` directly rather than hiding system state behind a metrics framework or shelling out to tools such as `ps`, `free`, or `df`.

## Project principles

- **Teaching first.** A number without meaning is not enough.
- **Your system is the textbook.** Lessons use live values and real processes from the current machine.
- **Low-level implementation.** Learn from Linux interfaces directly.
- **Small code over clever code.** Prefer the shortest correct implementation that remains readable.
- **Zero daemon, zero database.** One local process and one binary for the core monitor.
- **No root for normal monitoring.** RookieTop does not auto-escalate privileges.
- **Linux-first, distro-agnostic.** Depend on kernel interfaces, not distro package managers.
- **Measure before optimizing.** No complexity justified only by hypothetical performance.

## Alpha features

`0.1.0-alpha.6` includes:

- aggregate CPU usage from `/proc/stat`
- memory and swap from `/proc/meminfo` using `MemAvailable`
- root filesystem capacity through `statvfs()`
- aggregate non-loopback network throughput from `/proc/net/dev`
- host name, kernel release, online CPU count, uptime, and load average
- optional thermal-zone discovery from `/sys/class/thermal`
- top current CPU-consuming processes from `/proc/<pid>/stat`
- top memory-consuming processes from `/proc/<pid>/status`
- short CPU and memory activity history held in a fixed in-memory buffer
- full-screen ANSI terminal UI with raw `termios` + `poll` + `read` input
- all-process explorer with selection, sorting, inspection, and beginner-readable process states
- process details including PID, RSS, state, thread count, command line, and their procfs sources
- confirmed **Safe Stop** via SIGTERM and separately confirmed **Force Kill** via SIGKILL
- PID-reuse protection by verifying `/proc/<pid>/stat` start time immediately before signalling
- explicit permission / exited / PID-reused feedback instead of auto-sudo
- teaching catalog for CPU, memory, load, processes, PIDs, process states, signals, disk, and network
- contextual process lessons using the process currently selected by the user
- terminal-size-aware layout via `ioctl(TIOCGWINSZ)`
- alternate screen buffer and terminal cleanup on exit
- `NO_COLOR` and non-TTY / `--once` support

## Teaching model

Press `?` from RookieTop and choose a concept. Every lesson follows the same structure:

```text
WHAT
    What is this concept?

WHY IT MATTERS
    When should I care?

HOW LINUX / ROOKIETOP KNOWS
    Which procfs/sysfs/syscall interface exposes it?

TRY IT YOURSELF
    A small experiment to run on this machine.
```

For example, the CPU lesson does not stop at `CPU 32%`. It explains that Linux exposes cumulative counters in `/proc/stat`, that RookieTop calculates usage from two samples, and then suggests generating a temporary CPU load so the user can observe the metric change.

The process explorer works the same way. A selected `Sleeping` process is explained as a process normally waiting for work rather than being presented only as the raw `S` kernel state code. Press `?` on that process to connect its PID, RSS, state, and thread count back to `/proc/<pid>/`.

## Interactive keys

From the overview:

```text
? / l    Learn using live system data
p        Process Explorer
q        Quit
```

Inside Process Explorer:

```text
Up/Down  Select process
?        Explain the selected process
Enter    Inspect process and its Linux data sources
m        Sort by memory
p        Sort by PID
n        Sort by name
k        Safe Stop with SIGTERM
K        Force Kill with SIGKILL
Esc      Back
q        Quit
```

Inside the lesson browser:

```text
Up/Down  Choose concept
Enter    Open lesson
n        Next lesson
b        Previous lesson
Esc      Back
```

RookieTop never escalates SIGTERM to SIGKILL automatically. `K` is intentionally a separate action and confirmation because SIGKILL prevents the process from running cleanup handlers.

## Metric semantics

RookieTop deliberately teaches metrics that are easy to misread:

- **Memory** uses `MemAvailable`; Linux filesystem cache is not treated as wasted RAM.
- **Process CPU** is the process share of total machine CPU time during the sample window.
- **Load average** is queue pressure relative to online CPUs, not another CPU percentage.
- **Process state** is translated into beginner-readable meaning; for example, `Sleeping` is commonly a normal waiting state.
- **Thermal** is optional because many VMs and some systems do not expose thermal zones through sysfs.

See [`docs/SCOPE.md`](docs/SCOPE.md), [`docs/ROADMAP.md`](docs/ROADMAP.md), and [`docs/VM-TEST.md`](docs/VM-TEST.md).

## Build

Requirements:

- Linux
- a C11 compiler (`cc`, GCC, or Clang)
- `make`

```sh
make
./rookietop
```

For a non-interactive snapshot:

```sh
./rookietop --once
```

Disable terminal colors when needed:

```sh
NO_COLOR=1 ./rookietop
```

Developer checks:

```sh
make clean check
```

## Status

Alpha 6 establishes the first **Teaching Engine**. RookieTop now treats explanation and experimentation as first-class interaction rather than footer documentation. The larger visual redesign can continue from a dedicated TUI mockup without changing the teaching model or low-level collectors.

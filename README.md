# RookieTop

**Understand Linux by watching your own system.**

RookieTop is a beginner-first Linux system monitor and interactive teaching tool written in C. It uses the machine you are currently running as the example instead of separating monitoring from learning.

RookieTop tries to answer the questions a curious Linux beginner naturally asks:

1. **What is happening?**
2. **Should I care?**
3. **What is probably contributing to it?**
4. **How does Linux expose this information?**
5. **What can I try myself?**

It reads Linux interfaces such as `/proc`, `/sys`, `statvfs()`, and `kill(2)` directly rather than hiding system state behind a metrics framework or shelling out to tools such as `ps`, `free`, or `df`.

## Project principles

- **Plain language first.** Technical Linux terminology appears when it becomes useful, not before.
- **Progressive disclosure.** Overview -> explanation -> Linux internals -> experiment.
- **Your system is the example.** Explanations use live values and real processes from the current machine.
- **Evidence before diagnosis.** RookieTop describes what the current sample supports and avoids pretending that one metric proves a root cause.
- **Low-level implementation.** Learn from Linux interfaces directly.
- **Small code over clever code.** Prefer the shortest correct implementation that remains readable.
- **Zero daemon, zero database.** One local process and one binary for the core monitor.
- **No root for normal monitoring.** RookieTop does not auto-escalate privileges.
- **Long-session stability matters.** Leaving the monitor open must not progressively degrade the terminal or machine.

## Alpha features

`0.1.0-alpha.8` includes:

- aggregate CPU usage from `/proc/stat`
- memory and swap from `/proc/meminfo` using `MemAvailable`
- root filesystem capacity through `statvfs()`
- aggregate non-loopback network throughput from `/proc/net/dev`
- hostname, kernel release, online CPU count, uptime, and load average
- optional thermal-zone discovery from `/sys/class/thermal`
- top current CPU-consuming processes from `/proc/<pid>/stat`
- top memory-consuming processes from `/proc/<pid>/status`
- short CPU and memory activity history in a fixed in-memory buffer
- full-screen ANSI terminal UI with raw `termios` + `poll` + `read` input
- all-process explorer with selection, search, CPU/memory/PID/name sorting, inspection, and beginner-readable process states
- current whole-machine CPU share in the full process table
- confirmed **Stop safely** via SIGTERM and separately confirmed **Force kill** via SIGKILL
- PID-reuse protection by verifying process start time immediately before signalling
- teaching topics for CPU, memory, load, processes, PID, process states, signals, disk, and network
- contextual explanations based on the current system sample
- terminal-size-aware layout via `ioctl(TIOCGWINSZ)`
- alternate screen buffer and terminal cleanup on exit
- `NO_COLOR` and non-TTY / `--once` support
- a pseudo-terminal runtime stress harness with process churn, RSS/CPU sampling, and input-latency probes

## Runtime stress / soak testing

Run the short CI-style stress smoke:

```sh
make stress
```

Run longer soak qualification with the same harness:

```sh
ROOKIETOP_STRESS_SECONDS=1800 make stress
ROOKIETOP_STRESS_SECONDS=7200 make stress
ROOKIETOP_STRESS_SECONDS=28800 make stress
```

See [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md) for the long-session stability gate.

## Interactive keys

From the overview:

```text
? / l    Explain / explore concepts
p        Process Explorer
q        Quit
```

Inside Process Explorer:

```text
Up/Down  Move
/        Search by process name or PID
c        Sort by CPU
m        Sort by memory
p        Sort by PID
n        Sort by name
Enter    Details
?        Explain selected process
k        Stop safely with SIGTERM
K        Force kill with SIGKILL
Esc      Back
q        Quit
```

RookieTop never escalates SIGTERM to SIGKILL automatically. `K` is intentionally a separate action and warning.

## Metric semantics

- **Memory** uses `MemAvailable`; Linux filesystem cache is not treated as wasted RAM.
- **Process CPU** is a process share of whole-machine CPU time during the sampling window.
- **Load average** is queue/wait pressure relative to online CPUs, not another CPU percentage.
- **Process state** is translated into readable meaning; `Sleeping` is commonly a normal waiting state.
- **Thermal** is optional because many VMs do not expose thermal zones through sysfs.

See [`docs/SCOPE.md`](docs/SCOPE.md), [`docs/ROADMAP.md`](docs/ROADMAP.md), [`docs/VM-TEST.md`](docs/VM-TEST.md), and [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md).

## Build

Requirements:

- Linux
- a C11 compiler (`cc`, GCC, or Clang)
- `make`
- Python 3 only for the developer stress harness

```sh
make
./rookietop
```

One snapshot:

```sh
./rookietop --once
```

Disable colors:

```sh
NO_COLOR=1 ./rookietop
```

Developer checks:

```sh
make clean check
make stress
```

## Status

Alpha 8 completes the first usable Process Explorer interaction set. Current work is focused on runtime/soak stability before guided mini-labs, localization, and wider distro qualification.

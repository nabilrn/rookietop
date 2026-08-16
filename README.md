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

## Alpha features

`0.1.0-alpha.7` includes:

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
- all-process explorer with selection, sorting, inspection, and beginner-readable process states
- confirmed **Stop safely** via SIGTERM and separately confirmed **Force kill** via SIGKILL
- PID-reuse protection by verifying process start time immediately before signalling
- teaching topics for CPU, memory, load, processes, PID, process states, signals, disk, and network
- contextual explanations based on the current system sample
- high-CPU explanation that names the largest visible CPU contributor when the sample supports it
- high-memory explanation that shows the largest visible memory user without claiming it explains all memory use
- high-load explanation that distinguishes CPU saturation from possible I/O/kernel waiting
- relevant lesson preselection when the current system needs attention
- terminal-size-aware layout via `ioctl(TIOCGWINSZ)`
- alternate screen buffer and terminal cleanup on exit
- `NO_COLOR` and non-TTY / `--once` support

## Teaching flow

The default UI intentionally stays simple. Press `?` when you want to go deeper:

```text
SYSTEM VALUE
    CPU 92%

WHAT ROOKIETOP NOTICES
    The CPUs are very busy right now.
    gcc accounts for about 64% of whole-machine CPU in this sample.

?
    Explain CPU usage

WHAT IT MEANS
    Plain-language concept

WHEN TO CARE
    Useful context, not panic thresholds

HOW LINUX SHOWS IT
    /proc, /sys, or the relevant system call

TRY IT
    A small experiment on your own machine
```

The Process Explorer follows the same rule. The list shows readable labels such as `Sleeping` and a short **ABOUT THIS PROCESS** panel. PID reuse, RSS, procfs fields, and signal internals stay behind `? Explain` until the user asks for them.

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
Enter    Details
?        Explain selected process
m        Sort by memory
p        Sort by PID
n        Sort by name
k        Stop safely with SIGTERM
K        Force kill with SIGKILL
Esc      Back
q        Quit
```

RookieTop never escalates SIGTERM to SIGKILL automatically. `K` is intentionally a separate action and warning.

## Metric semantics

- **Memory** uses `MemAvailable`; Linux filesystem cache is not treated as wasted RAM.
- **Process CPU** is a process share of whole-machine CPU time during the short sample window.
- **Load average** is queue/wait pressure relative to online CPUs, not another CPU percentage.
- **Process state** is translated into readable meaning; `Sleeping` is commonly a normal waiting state.
- **Thermal** is optional because many VMs do not expose thermal zones through sysfs.

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
```

## Status

Alpha 7 adds the first contextual diagnosis layer and a full copywriting pass around progressive disclosure. The next major product work is process search/CPU sorting, guided mini-labs, localization, distro qualification, and a dedicated final TUI visual redesign.

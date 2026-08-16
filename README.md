# RookieTop

RookieTop is a beginner-first Linux system monitor written in C.

It aims to answer three questions clearly:

1. **What is happening?**
2. **Should I care?**
3. **How does Linux expose this information?**

RookieTop reads Linux interfaces such as `/proc` and small POSIX/Linux APIs directly instead of hiding system state behind a metrics framework.

## Project principles

- **Beginner-first output.** Human meaning before raw numbers.
- **Low-level implementation.** Learn from Linux interfaces directly.
- **Small code over clever code.** Prefer the shortest correct implementation that remains readable.
- **Zero daemon, zero database.** One local process and one binary for the core monitor.
- **No root for normal monitoring.** Privileged features are not part of the core experience.
- **Linux-first, distro-agnostic.** Depend on kernel interfaces, not distro package managers.
- **Measure before optimizing.** No complexity justified only by hypothetical performance.

## Alpha features

`0.1.0-alpha.1` currently includes:

- aggregate CPU usage from `/proc/stat`
- memory and swap from `/proc/meminfo` using `MemAvailable`
- root filesystem capacity through `statvfs()`
- aggregate non-loopback network throughput from `/proc/net/dev`
- top memory-consuming processes from `/proc/<pid>/status`
- beginner-readable health labels and explanations
- live ANSI refresh in an interactive terminal
- one-shot output for scripts and CI

Per-process CPU ranking, localization, temperature, and multi-distro hardening are still pending.

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

In an interactive terminal, `./rookietop` refreshes roughly once per second. Press `Ctrl+C` to quit.

For one snapshot:

```sh
./rookietop --once
```

Developer checks:

```sh
make clean check
```

## Status

The first manual-test alpha is ready for Linux VM validation. The next work should be driven by real VM observations before adding more collector complexity.

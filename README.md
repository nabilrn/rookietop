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

`0.1.0-alpha.3` currently includes:

- aggregate CPU usage from `/proc/stat`
- memory and swap from `/proc/meminfo` using `MemAvailable`
- root filesystem capacity through `statvfs()`
- aggregate non-loopback network throughput from `/proc/net/dev`
- top memory-consuming processes from `/proc/<pid>/status`
- full-screen interactive terminal dashboard using ANSI escape sequences
- terminal-size-aware layout via `ioctl(TIOCGWINSZ)`
- alternate screen buffer so the previous shell screen is restored on exit
- compact resource bars and semantic health colors
- concise beginner-readable insight
- automatic terminal cleanup on `Ctrl+C` / `SIGTERM`
- compact fallback for terminals smaller than 80x24
- one-shot output for scripts and CI
- `NO_COLOR` support and plain non-TTY output

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

In an interactive terminal, `./rookietop` opens a full-screen dashboard and refreshes roughly once per second. Press `Ctrl+C` to quit; the original terminal screen is restored automatically.

For one snapshot:

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

The alpha is being validated on a real Linux VM. Alpha 3 focuses on making interactive monitoring feel like a proper full-screen terminal application without adding ncurses or another TUI dependency.

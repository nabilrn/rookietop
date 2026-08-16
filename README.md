# RookieTop

RookieTop is a beginner-first Linux system monitor written in C.

It aims to answer three questions clearly:

1. **What is happening?**
2. **Should I care?**
3. **How does Linux expose this information?**

RookieTop intentionally reads Linux interfaces such as `/proc`, `/sys`, and small POSIX/Linux APIs directly instead of hiding them behind a system-metrics framework.

## Project principles

- **Beginner-first output.** Human meaning before raw numbers.
- **Low-level implementation.** Learn from Linux interfaces directly.
- **Small code over clever code.** Prefer the shortest correct implementation that remains readable.
- **Zero daemon, zero database.** One local process and one binary for the core monitor.
- **No root for normal monitoring.** Privileged features are not part of the core experience.
- **Linux-first, distro-agnostic.** Depend on kernel interfaces, not distro package managers.
- **Measure before optimizing.** No complexity justified only by hypothetical performance.

## Initial scope

The first usable release will cover:

- system overview
- CPU
- memory
- disk/filesystem
- network
- processes
- simple health explanations
- English and Indonesian text

See [`docs/SCOPE.md`](docs/SCOPE.md) and [`docs/ROADMAP.md`](docs/ROADMAP.md) for the boundaries and delivery plan.

## Build

Requirements:

- Linux
- a C11 compiler (`cc`, GCC, or Clang)
- `make`

```sh
make
./rookietop
```

Developer checks:

```sh
make check
```

## Status

Phases 1 and 2 are implemented. RookieTop reads CPU accounting from `/proc/stat` and memory state from `/proc/meminfo`, using `MemAvailable` so Linux filesystem cache is not mistaken for wasted memory. Disk and network are next.

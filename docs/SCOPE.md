# RookieTop Scope

## Product statement

RookieTop is a local Linux system monitor for beginners who want to understand what their machine is doing without first learning the vocabulary of `top`, `htop`, Grafana, or a full observability stack.

The product must present **meaning first** and raw kernel/system data second.

## Primary user

A Linux beginner, student, homelab user, junior developer, or junior infrastructure engineer who asks questions such as:

- Is my system healthy?
- Why is it slow?
- Is high RAM usage bad?
- Which process is consuming resources?
- Where did this metric come from?

## v0.1 scope

### 1. Overview

Show only the most useful current signals:

- CPU usage
- memory usage
- root filesystem usage
- network receive/transmit rate
- uptime
- overall health summary

### 2. CPU

Source primarily from `/proc/stat` and `/proc/loadavg`.

Show:

- total CPU usage
- optional per-core usage
- load average with a beginner explanation
- top CPU-consuming processes

### 3. Memory

Source primarily from `/proc/meminfo` and process data under `/proc/<pid>`.

Show:

- total memory
- available memory
- used memory
- swap usage
- top memory-consuming processes

Prefer `MemAvailable`-based explanations over the misleading idea that all non-free RAM is a problem.

### 4. Disk/filesystem

Use a small libc/POSIX/Linux API such as `statvfs()` for filesystem capacity. Device I/O is a later enhancement.

Show:

- filesystem capacity
- used/free space
- warning when free space becomes low

### 5. Network

Source initially from `/proc/net/dev` and `/sys/class/net`.

Show:

- active interfaces
- receive rate
- transmit rate
- link state where available

### 6. Processes

Read directly from `/proc/<pid>`.

Default view should emphasize the processes that matter rather than dumping every field.

Show:

- PID
- process name
- CPU usage
- memory usage
- state in plain language

### 7. Explanation layer

Each important signal can have:

- `Normal`, `Attention`, or `Critical` status
- one short human explanation
- an optional technical explanation
- the Linux source used to derive the value

Thresholds are heuristics, not medical-style truth. Wording must avoid claiming that one high sample proves a fault.

### 8. Localization

Initial languages:

- English
- Indonesian

Text must be separated from collection logic. Do not duplicate collectors or diagnosis rules per language.

## Platform scope

### Supported

Linux systems that expose normal procfs/sysfs interfaces.

The implementation should be distro-agnostic for core monitoring. Ubuntu, Debian, Fedora, Arch, Rocky/Alma, and openSUSE should not require separate collectors for standard metrics.

### Not initially supported

- Windows
- macOS
- BSD
- Android
- distro package management
- service management abstractions

## Explicit non-goals for v0.1

- Grafana/Prometheus compatibility
- remote fleet monitoring
- long-term metric storage
- database
- daemon/service
- web server or browser UI
- plugin system
- eBPF
- perf events
- container orchestration monitoring
- package-manager integration
- process killing from the beginner view
- ncurses or another TUI framework unless ANSI rendering proves insufficient

## Product boundary

RookieTop is not trying to replace `htop`, `btop`, Netdata, Prometheus, or Grafana.

Its niche is:

> **See what Linux is doing, understand whether it matters, and learn where the answer came from.**

# RookieTop Roadmap

The roadmap is intentionally narrow. Each phase should leave a working program and should not introduce infrastructure needed only by later phases.

## Phase 0 — Foundation

Goal: establish the smallest reliable C project.

- [x] project vision and scope
- [x] minimal C11 CLI entrypoint
- [x] Makefile with strict warnings
- [x] sanitizer debug build
- [x] CI compile and smoke checks
- [x] C systems-programming agent rules
- [x] verify bootstrap CI on GitHub Actions

## Phase 1 — CPU + sampling model

- [x] parse aggregate CPU counters from `/proc/stat`
- [x] calculate usage from two samples
- [x] show current CPU percentage
- [x] explain that CPU usage is derived from counter deltas
- [x] unit-test parsing and delta math with fixtures

## Phase 2 — Memory + beginner semantics

- [x] parse `/proc/meminfo`
- [x] use `MemAvailable` semantics
- [x] show total, available, used, and swap
- [x] provide conservative beginner explanation
- [x] fixture tests for common and missing fields

## Phase 3 — Disk + network

- [x] root filesystem capacity using `statvfs()`
- [x] network counters from `/proc/net/dev`
- [x] calculate RX/TX rates from sample deltas
- [x] degrade gracefully when counters regress or interface state changes

## Phase 4 — Processes

- [x] enumerate numeric `/proc/<pid>` entries
- [x] parse only fields RookieTop displays
- [x] rank top memory consumers with a fixed-size list
- [x] sample and rank current CPU consumers from `/proc/<pid>/stat`
- [x] guard PID reuse with process start time
- [x] handle process churn without noisy errors
- [x] explain that per-process CPU is total-machine share
- [x] interactive all-process explorer
- [x] process detail view with state, threads, RSS, and command line
- [x] sort process explorer by memory, PID, or name
- [x] confirmed SIGTERM action
- [x] separately confirmed SIGKILL action
- [x] verify process start time again immediately before signalling
- [ ] process name search/filter
- [ ] all-process CPU column and CPU sorting

The CPU sampler uses short-lived allocation only to match PIDs across the sampling window; it does not keep a daemon-style process database. Process control never auto-escalates privileges or SIGTERM to SIGKILL.

## Phase 5 — Beginner dashboard

- [x] single-screen overview
- [x] beginner-readable health labels
- [x] concise primary insight
- [x] compact CPU / memory / disk resource bars
- [x] semantic terminal colors with `NO_COLOR` fallback
- [x] full-screen alternate-buffer live mode without a TUI framework
- [x] terminal-size-aware responsive layout
- [x] top CPU and top memory panels
- [x] fixed-memory short CPU / memory activity history
- [x] host, kernel, uptime, load, and optional thermal context
- [x] non-TTY and `--once` one-shot fallback
- [x] raw keyboard input using termios/poll/read
- [ ] dedicated visual redesign from a TUI mockup
- [ ] optional interactive explanation / `? Why?` view

## Phase 6 — Localization

- [ ] English text catalog
- [ ] Indonesian text catalog
- [ ] locale selection via CLI and/or environment
- [ ] English fallback for unknown/missing locale
- [ ] prevent collector/diagnosis duplication by language

## Phase 7 — v0.1 hardening

- [ ] test Ubuntu/Debian family
- [ ] test Fedora/RHEL family
- [ ] test Arch family
- [ ] test missing/partial sysfs features
- [ ] measure idle CPU and RSS
- [ ] document supported Linux assumptions
- [ ] release tested Linux binaries

## Deferred until justified

- ncurses
- threads
- eBPF
- perf events
- remote monitoring
- metric persistence
- Prometheus export
- Docker/Kubernetes awareness
- plugin system

A deferred item moves into scope only with a concrete user requirement and a design showing why the simpler current approach is insufficient.

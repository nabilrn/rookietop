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

## Phase 4 — Processes + safe actions

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
- [x] confirmed Safe Stop with SIGTERM
- [x] separately confirmed Force Kill with SIGKILL
- [x] verify process start time immediately before signalling
- [x] explain SIGTERM vs SIGKILL before an action
- [x] process name/PID search and filter
- [x] all-process CPU column and CPU sorting
- [x] preserve selected process across sort/refresh using PID + start time

The CPU sampler uses short-lived allocation only to match PIDs across the sampling window; it does not keep a daemon-style process database. Process control never auto-escalates privileges or SIGTERM to SIGKILL. Process search is an in-memory substring filter and does not shell out to another command.

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
- [x] progressive-disclosure copy pass: plain language before Linux internals
- [ ] final UX pass from real beginner testing

## Phase 5.5 — Teaching Engine

Goal: make RookieTop a Linux learning environment, not merely a monitor with explanatory labels.

- [x] `?` / `l` learning entry point from live overview
- [x] static teaching catalog kept separate from collectors
- [x] WHAT → WHY → HOW → TRY lesson structure
- [x] lessons for CPU, memory, load average, processes, PID, process states, signals, disk, and network
- [x] show live machine values inside relevant lessons
- [x] expose the actual procfs/sysfs/syscall data source in each lesson
- [x] provide a small local experiment for each concept
- [x] translate raw process-state codes into beginner-readable states
- [x] selected-process teaching panel in Process Explorer
- [x] contextual process lesson using the selected PID/state/RSS/threads
- [x] Process Inspector connects fields back to `/proc/<pid>/`
- [x] Safe Stop / Force Kill confirmations teach signal semantics
- [x] teaching catalog tests and sanitizer coverage
- [x] context-aware explanation for CPU, memory, disk, and load pressure
- [x] connect high CPU to a visible top contributor when supported by the sample
- [x] connect high memory to the largest visible memory user without claiming full causality
- [x] distinguish high load with low CPU from likely CPU queueing
- [x] preselect the most relevant lesson from current system evidence
- [x] deterministic contextual-diagnosis tests and sanitizer coverage
- [ ] guided mini-labs that observe whether the experiment changed the real system

Teaching and diagnosis must not duplicate collector logic. They consume normalized live data, explain the evidence conservatively, and expose the Linux source only when the user asks to go deeper.

## Phase 6 — Localization

- [ ] English text catalog
- [ ] Indonesian text catalog
- [ ] locale selection via CLI and/or environment
- [ ] English fallback for unknown/missing locale
- [ ] prevent collector/diagnosis duplication by language

## Phase 7 — Stable hardening

- [ ] test Ubuntu/Debian family
- [ ] test Fedora/RHEL family
- [ ] test Arch family
- [ ] test missing/partial sysfs features
- [ ] measure idle CPU and RSS
- [ ] document supported Linux assumptions
- [ ] qualify common terminal sizes and SSH use
- [ ] release tested x86_64 and arm64 Linux binaries

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

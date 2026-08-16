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
- [ ] verify bootstrap CI on GitHub Actions

Exit gate: GCC and Clang build cleanly and `make check` passes.

## Phase 1 — CPU + sampling model

Goal: prove the low-level collector architecture with one useful metric.

- [ ] parse aggregate CPU counters from `/proc/stat`
- [ ] calculate usage from two samples
- [ ] show current CPU percentage
- [ ] explain that CPU usage is derived from counter deltas
- [ ] unit-test parsing and delta math with fixtures

Exit gate: CPU calculation is correct against known fixtures and works on a real Linux host without root.

## Phase 2 — Memory + beginner semantics

Goal: demonstrate that RookieTop explains Linux rather than merely displaying numbers.

- [ ] parse `/proc/meminfo`
- [ ] use `MemAvailable` semantics
- [ ] show total, available, used, and swap
- [ ] provide conservative beginner explanation
- [ ] fixture tests for common and missing fields

Exit gate: the UI does not equate Linux cache with a memory problem.

## Phase 3 — Disk + network

- [ ] root filesystem capacity using `statvfs()`
- [ ] network counters from `/proc/net/dev`
- [ ] calculate RX/TX rates from sample deltas
- [ ] degrade gracefully when interfaces appear/disappear

Exit gate: rates are stable, bounded, and do not require subprocesses.

## Phase 4 — Processes

- [ ] enumerate numeric `/proc/<pid>` entries
- [ ] parse only fields RookieTop displays
- [ ] rank top CPU and memory consumers
- [ ] handle process churn without noisy errors
- [ ] expose technical source/detail view

Exit gate: the default UI remains readable on hosts with many processes.

## Phase 5 — Beginner dashboard

- [ ] single-screen overview
- [ ] `Normal` / `Attention` / `Critical` presentation
- [ ] optional explanation/detail interaction
- [ ] ANSI terminal handling with no required TUI framework
- [ ] usable monochrome fallback

Exit gate: a first-time Linux user can identify the main resource pressure without understanding process-monitor jargon.

## Phase 6 — Localization

- [ ] English text catalog
- [ ] Indonesian text catalog
- [ ] locale selection via CLI and/or environment
- [ ] English fallback for unknown/missing locale
- [ ] prevent collector/diagnosis duplication by language

Exit gate: both languages expose the same metrics and behavior.

## Phase 7 — v0.1 hardening

- [ ] test Ubuntu/Debian family
- [ ] test Fedora/RHEL family
- [ ] test Arch family
- [ ] test missing/partial sysfs features
- [ ] measure idle CPU and RSS
- [ ] document supported Linux assumptions
- [ ] release one Linux binary per supported architecture only when actually tested

Exit gate: no distro-specific core collector is required for supported metrics, CI is green, and measured overhead is documented.

## Deferred until justified

- ncurses
- threads
- eBPF
- perf events
- remote monitoring
- metric persistence
- Prometheus export
- Docker/Kubernetes awareness
- process termination controls
- plugin system

A deferred item moves into scope only with a concrete user requirement and a design showing why the simpler current approach is insufficient.

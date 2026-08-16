# RookieTop Architecture

## Design goal

Keep the runtime and code path obvious enough that a beginner can trace a displayed value back to the Linux interface that produced it.

```text
Linux kernel interfaces
    |
    |  /proc, /sys, small syscalls/APIs
    v
collector
    |
    v
plain metric struct
    |
    +--> diagnosis/explanation
    |
    v
terminal renderer
```

## Dependency rule

The preferred dependency order is:

1. C standard library
2. POSIX/Linux interfaces already available on the host
3. a small external dependency only when a concrete requirement cannot be met simply and safely without it

A dependency must not be added merely to avoid writing a small parser for a stable Linux text interface.

## Module growth

Do not pre-create abstractions. Split a source file only when it has a real independent responsibility.

Expected growth path:

```text
src/
  main.c
  cpu.c
  memory.c
  disk.c
  network.c
  process.c
  explain.c
  render.c
  locale.c
```

Headers should exist only for interfaces used by more than one translation unit. Do not mirror every `.c` file with a header automatically.

## Collectors

Collectors should:

- read one Linux interface or one tightly related group of interfaces
- validate parsing
- return normalized values, not formatted UI strings
- avoid hidden global state
- avoid heap allocation when bounded stack storage is simpler and safe
- retain only the previous sample required for rate/delta calculations

Collectors should not:

- decide colors or UI layout
- translate human text
- spawn helper shell commands to obtain metrics that Linux already exposes directly
- parse command output when a kernel/userspace interface is available

## Diagnosis

Diagnosis converts normalized metrics into conservative status values and explanation identifiers.

```text
metric -> rule -> status + explanation key
```

Rules must remain deterministic and small. A single high sample should generally be described as a current condition, not diagnosed as a permanent fault.

## Rendering

The first renderer uses normal terminal output and ANSI escape sequences only where useful.

Do not add ncurses before the project demonstrably needs capabilities that are difficult to implement cleanly with simple terminal control.

The UI must remain usable when color is unavailable.

## Localization

Collection and diagnosis logic must never branch on human language.

```text
explanation key -> locale lookup -> text
```

English is the fallback locale. Indonesian is the second initial locale.

## Runtime model

The initial runtime is a single process and a single sampling loop.

No threads, async runtime, worker pool, daemon, IPC, or event bus should be introduced until a measured requirement makes the single-loop design insufficient.

## Error handling

- Check return values from syscalls and libc I/O.
- Treat disappearing `/proc/<pid>` entries as normal process churn.
- Prefer explicit error codes over elaborate error frameworks.
- Fail gracefully when an optional metric is unavailable.
- Never invent a value when the source cannot be read.

## Performance philosophy

RookieTop should be lightweight because it does little work, not because it contains clever micro-optimizations.

Priorities:

1. correctness
2. bounded work per refresh
3. low allocation count
4. readable code
5. measured optimization

If a simpler implementation is already far below the project's CPU/memory budget, keep it simple.

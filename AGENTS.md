# RookieTop Agent Instructions

These rules apply to every automated coding agent working in this repository.

## Mission

Build a beginner-first Linux system monitor in C that is easy to understand both as a user and as a reader of the source code.

The project values **correct small code** over architectural sophistication.

## Required context

Before implementing or reviewing C code, read:

1. `docs/SCOPE.md`
2. `docs/ARCHITECTURE.md`
3. `docs/ROADMAP.md`
4. `.agents/skills/c-systems/SKILL.md`

Do not implement a deferred roadmap item unless the task explicitly brings it into scope.

## Non-negotiable engineering rules

- Use C11 unless a concrete Linux API requires an explicitly documented extension.
- Prefer libc, POSIX, procfs, sysfs, and direct Linux APIs over third-party libraries.
- Do not add a dependency for functionality that can be implemented clearly in a small amount of local C.
- Do not shell out to `top`, `ps`, `free`, `df`, `ip`, `ss`, or similar tools to collect metrics already available through Linux interfaces.
- Do not introduce threads, async/event frameworks, generic plugin systems, dependency injection, object-style frameworks, or abstraction layers speculatively.
- Avoid heap allocation when a small bounded stack buffer or caller-owned buffer is simpler and safe.
- Do not optimize based on intuition alone. Measure first.
- Never ignore syscall/libc return values relevant to correctness.
- Gracefully handle optional/missing Linux interfaces.
- Treat `/proc/<pid>` races as normal process churn.
- Keep collection, diagnosis, localization, and rendering responsibilities separate only where the separation is actually needed.

## Change size

Prefer one narrow behavior per branch/PR. If a change can be implemented safely in one small function, do not create a subsystem for it.

Every new source file, public interface, dependency, thread, global mutable object, or heap allocation should have an obvious reason visible in the code or PR.

## Verification

Before considering a C change complete:

```sh
make clean
make
make check
make debug
./rookietop --help
```

For parser/math changes, add deterministic tests or fixtures before relying on live `/proc` output alone.

Warnings are errors. Do not suppress a warning unless the code is demonstrably correct and the suppression is narrowly documented.

## Git workflow

- `main` should remain releasable/buildable.
- Use short-lived branches such as `feat/...`, `fix/...`, `test/...`, `docs/...`, or `chore/...`.
- Do not mix unrelated refactors into feature work.
- Prefer small reviewable commits with descriptive messages.

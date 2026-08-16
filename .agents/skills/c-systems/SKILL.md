# C Systems Programming Skill

Use this skill whenever implementing, modifying, or reviewing C code in RookieTop.

## Objective

Produce the **smallest correct implementation** that reads Linux system state efficiently and remains easy for another programmer to audit.

Do not pursue cleverness, genericity, or theoretical extensibility.

## Decision order

When choosing an implementation, prefer in this order:

1. a direct standard C solution
2. a small POSIX/Linux API call
3. a small parser for a stable procfs/sysfs interface
4. a narrowly scoped helper function
5. a new module only when the responsibility is independently meaningful
6. an external dependency only after demonstrating why the above are insufficient

## C rules

### Keep ownership obvious

- Prefer automatic storage for small fixed-size values and buffers.
- Prefer caller-owned output structs/buffers over hidden allocation.
- If heap allocation is necessary, ownership and lifetime must be obvious at the call site.
- Every successful allocation must have one clear release path.

### Keep control flow boring

- Prefer early returns for invalid input/errors.
- Prefer a straight-line parser over callback frameworks or generic parser layers.
- Avoid recursion unless the problem is naturally recursive and bounded.
- Avoid macro metaprogramming for normal control flow or data structures.
- Use macros mainly for constants or tiny compile-time utilities.

### Keep types honest

- Use fixed-width integer types when the width matters.
- Use unsigned arithmetic only when wraparound semantics are actually acceptable.
- Check narrowing conversions where kernel counters or file sizes may exceed the destination type.
- Use `size_t` for object/buffer sizes.
- Do not encode errors in legitimate metric values when a return status is clearer.

### Parse defensively

For procfs/sysfs text:

- validate that required fields were actually read
- tolerate extra fields from newer kernels where practical
- reject malformed numeric input instead of silently producing zero
- do not assume a single read returns all logically available data unless the interface guarantees the bounded read is sufficient
- handle counters that can change between samples
- handle files/processes disappearing between enumeration and read

### Buffers

- Use bounded functions and explicit capacities.
- Never use `gets`, unbounded `%s`, or unchecked string copies.
- Prefer `snprintf` when formatting into fixed buffers.
- Ensure strings are terminated on every successful path.
- Do not allocate huge buffers for small procfs records.

## Linux interface rules

### `/proc`

Use `/proc` for kernel/process counters and process metadata when it is the canonical simple source.

Examples:

- `/proc/stat` — CPU accounting
- `/proc/meminfo` — memory state
- `/proc/loadavg` — load average
- `/proc/uptime` — uptime
- `/proc/net/dev` — basic interface counters
- `/proc/<pid>/...` — process metrics

Do not invoke shell utilities to reformat data already available here.

### `/sys`

Use sysfs for device/kernel object attributes such as link state or thermal information.

Discover entries instead of assuming names like `thermal_zone0` always mean the same hardware.

### libc/POSIX/Linux APIs

Prefer a direct API when it is clearer than parsing a pseudo-file. Example: filesystem capacity via `statvfs()`.

Use Netlink, `ioctl`, `epoll`, `perf_event_open`, or eBPF only when a scoped feature genuinely requires them. Their existence is not a reason to use them.

## Sampling rules

Many rates are derived, not directly reported.

For a delta-based metric:

1. read sample A
2. wait for the normal refresh interval
3. read sample B
4. validate monotonicity/reset conditions
5. calculate the delta using wide integer types
6. convert to a human value only after the raw calculation is correct

Store only the prior sample needed for the next calculation.

## Error model

Use simple return conventions consistently:

- `0` for success and a non-zero status for failure, or
- `bool` where there is only success/failure and no useful error classification

Do not build a generic error object hierarchy.

Optional metrics should fail locally. Missing temperature data, for example, must not make CPU/memory monitoring unusable.

## Performance rules

RookieTop is lightweight by doing bounded direct work.

Before optimizing, ask:

1. Is this code on every refresh?
2. Is the current cost measurable?
3. Is the proposed implementation simpler or demonstrably faster?
4. Does it increase memory-safety or parsing risk?

Prefer:

- one pass over input
- bounded buffers
- reuse of previous samples
- no subprocesses
- no unnecessary allocation/copying

Do not introduce caches, pools, lock-free structures, SIMD, custom allocators, threads, or mmap tricks without measurements and a concrete need.

## API design

Keep internal APIs narrow.

Good shape:

```c
int cpu_read(struct cpu_sample *out);
double cpu_usage(const struct cpu_sample *prev,
                 const struct cpu_sample *curr);
```

Avoid generic shapes such as:

```c
metric_provider_register(...);
collector_factory_create(...);
event_bus_publish(...);
```

unless the project later develops a real requirement that cannot be handled simply.

## Testing strategy

Do not test parsers only against the developer's live machine.

For each parser or calculation:

- keep representative tiny fixtures under `tests/fixtures/`
- test valid input
- test malformed/truncated input where meaningful
- test zero/delta edge cases
- test counter reset/wrap assumptions where relevant

Live-host checks complement deterministic tests; they do not replace them.

## Review checklist

Reject or simplify code when any answer below is unclear:

- What Linux interface supplies this value?
- Who owns every buffer/allocation?
- What happens when the source is missing or malformed?
- Can a process/device disappear during this operation?
- Is integer overflow/conversion handled?
- Is this abstraction used by more than one real case?
- Could the same behavior be implemented more clearly with fewer moving parts?
- Has a claimed optimization been measured?

## Definition of done

A C change is done only when:

- behavior matches the scoped requirement
- code builds with project warning flags
- deterministic logic has tests/fixtures when appropriate
- sanitizer build succeeds
- no unnecessary dependency or architecture was introduced
- documentation is updated only when behavior/contracts changed

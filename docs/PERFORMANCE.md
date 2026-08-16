# RookieTop Runtime Stability Qualification

Long-session responsiveness is a release requirement. A monitor that gradually makes its own terminal sluggish is not stable, even if its metrics remain correct.

## Automated stress harness

`tests/stress_runtime.py` runs RookieTop inside a pseudo-terminal, enters Process Explorer, creates continuous short-lived process churn, repeatedly switches CPU/memory sorting, and samples RookieTop itself from `/proc`.

```sh
make stress
```

The default run is intentionally short enough for CI. It verifies that:

- RookieTop survives process churn
- sort-key input remains responsive
- RSS growth from a warm baseline stays bounded
- average CPU usage does not run away
- `q` still exits promptly

These limits are regression guards, not performance marketing claims.

## Soak tests

Use the exact same harness for longer qualification runs on a real VM:

```sh
ROOKIETOP_STRESS_SECONDS=1800 make stress   # 30 minutes
ROOKIETOP_STRESS_SECONDS=7200 make stress   # 2 hours
ROOKIETOP_STRESS_SECONDS=28800 make stress  # 8 hours
```

The harness reports starting, ending, and peak RSS; average RookieTop CPU; maximum measured sort-key response latency; and the number of probes completed.

## Regression limits

Defaults:

```text
RSS growth from warm baseline   <= 8192 KiB
sort-key response latency       <= 2.5 s
average RookieTop CPU           <= 50%
```

They can be overridden for diagnosis:

```sh
ROOKIETOP_STRESS_MAX_RSS_GROWTH_KIB=8192
ROOKIETOP_STRESS_MAX_INPUT_LATENCY_SECONDS=2.5
ROOKIETOP_STRESS_MAX_AVG_CPU_PERCENT=50
ROOKIETOP_STRESS_CHURN_INTERVAL_SECONDS=0.10
```

Do not relax a threshold merely to turn CI green. First determine whether the host is unusually constrained or RookieTop is doing unnecessary work.

## v1.0 soak gate

Before v1.0, record at least one 8-hour run with:

- distro and kernel
- local terminal vs SSH
- approximate process count
- starting / ending / peak RSS
- average RookieTop CPU
- maximum input-response latency
- whether the display becomes visually delayed over time

The required behavior is simple: **leaving RookieTop open must not progressively degrade the machine or terminal experience.**

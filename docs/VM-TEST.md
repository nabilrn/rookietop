# RookieTop Alpha VM Test

This checklist qualifies the current `0.1.0-alpha.6` Linux build on a real VM.

## 1. Record the environment

```sh
cat /etc/os-release
uname -a
nproc
free -h
df -h /
cat /proc/loadavg
```

Keep the distro, kernel, CPU count, RAM, and root filesystem size with the test notes.

## 2. Build and automated checks

```sh
git clone https://github.com/nabilrn/rookietop.git
cd rookietop
make clean check
```

Expected: strict compilation and all tests pass. The suite includes collector/parser tests, live process inspection, guarded signalling, and the static teaching catalog.

## 3. One-shot smoke test

```sh
./rookietop --once
```

Check that CPU, memory, root disk, network, and the primary insight render without root privileges.

## 4. Teaching-first overview

```sh
./rookietop
```

Expected:

- alternate full-screen terminal buffer is used
- shell and terminal input mode are restored on exit
- the overview visibly presents RookieTop as learning from this machine, not only as a metric dashboard
- `UNDERSTAND` prompts the user to ask why metrics work the way they do
- `[?] Learn` is visually discoverable in the action bar
- `p` still opens Process Explorer

## 5. Lesson browser

From the overview press `?` or `l`.

Expected:

- `LEARN LINUX WITH YOUR SYSTEM` opens
- current CPU, memory, disk, and load context is shown when available
- Up/Down changes the selected lesson
- Enter opens the selected lesson
- lessons exist for CPU, Linux memory, load average, processes, PID, process state, signals, filesystem space, and network throughput
- Esc returns to the previous screen

## 6. WHAT / WHY / HOW / TRY contract

Open several lessons, especially CPU, memory, and load.

Every lesson must visibly contain:

```text
WHAT
WHY IT MATTERS
HOW LINUX / ROOKIETOP KNOWS
TRY IT YOURSELF
```

Check that the lesson also names its real data source. Examples:

- CPU -> `/proc/stat`
- memory -> `/proc/meminfo`
- load -> `/proc/loadavg`
- disk -> `statvfs("/")`
- network -> `/proc/net/dev`

Use `n` and `b` to move between lessons.

## 7. CPU teaching experiment

Open the CPU lesson. In another shell:

```sh
yes > /dev/null &
LOAD_PID=$!
```

Return to the overview after observing the lesson and watch CPU usage. Then stop the test:

```sh
kill "$LOAD_PID"
```

Expected: the experiment described by RookieTop makes the monitored value visibly change. The lesson should make clear that CPU percentage is calculated from cumulative `/proc/stat` counter deltas.

## 8. Memory teaching sanity check

Open the memory lesson and compare it with:

```sh
grep -E 'MemTotal|MemAvailable|MemFree|Cached' /proc/meminfo
```

Expected: RookieTop teaches that used RAM is not equivalent to memory pressure and that `MemAvailable` is more useful than treating `MemFree` as all usable memory.

## 9. Process Explorer as teaching UI

Press `p`.

Check:

- column label is `MEM MiB` instead of unexplained `RSS MiB`
- raw process state `S` is shown as `Sleeping`, `R` as `Running`, etc.
- the selected process has a visible `LEARN SELECTED` area
- the selected state is explained in plain language; `Sleeping` should not be framed as an error
- `? Explain`, `Enter Inspect`, `k Safe Stop`, and `K Force Kill` are clearly discoverable
- `m`, `p`, and `n` still sort by memory, PID, and name

## 10. Contextual process lesson

Select a stable process and press `?`.

Expected:

- the lesson uses the actual selected process name, PID, state, RSS, and thread count
- WHAT explains a process and PID
- WHY explains the selected process state
- HOW points to `/proc/<pid>/status`, `/proc/<pid>/stat`, and `/proc/<pid>/cmdline`
- TRY asks the user to inspect `/proc/<pid>/status` manually

In another shell, run the suggested command using the same PID and compare `State`, `Threads`, and `VmRSS` with RookieTop.

## 11. Process Inspector

Return to Process Explorer, select a process, and press Enter.

Expected:

- PID, status, memory, and threads include inline beginner meaning
- command line is displayed
- procfs paths used by RookieTop are shown explicitly
- the screen explains why process start time matters for PID reuse protection
- `?` opens the contextual process lesson

## 12. Safe Stop teaching

Create a disposable process:

```sh
sleep 300 &
TEST_PID=$!
echo "$TEST_PID"
```

Select it in Process Explorer and press `k`.

Before confirming, verify the screen explains:

- SIGTERM requests graceful shutdown
- software may clean up before exiting
- RookieTop calls `kill(2)` directly
- PID + start time are verified first
- RookieTop will not auto-escalate to SIGKILL

Press `y`. The disposable process should exit.

## 13. Force Kill teaching

Use only a disposable process that ignores SIGTERM:

```sh
python3 -c 'import signal,time; signal.signal(signal.SIGTERM, signal.SIG_IGN); time.sleep(300)' &
TEST_PID=$!
echo "$TEST_PID"
```

Try `k` + `y` first. The process should remain alive. Re-select it and press uppercase `K`.

Expected: the Force Kill confirmation clearly states that SIGKILL is immediate, cannot be caught, and skips cleanup. Only explicit `y` sends it.

## 14. Load-average behavior

Compare RookieTop with:

```sh
cat /proc/loadavg
nproc
```

Expected: load values match closely and the lesson describes load as queue pressure relative to CPU count, not another CPU percentage.

## 15. Network, thermal, and disk regression checks

Generate a normal download/package update and verify network rate moves and falls again.

Check optional thermal exposure:

```sh
ls /sys/class/thermal/thermal_zone*/temp 2>/dev/null
```

Compare root filesystem capacity:

```sh
df -h /
```

Missing thermal zones must remain non-fatal.

## Report back

Record:

- distro and kernel
- `make clean check` result
- screenshot of teaching-first overview
- screenshot of lesson browser and one full WHAT/WHY/HOW/TRY lesson
- screenshot of Process Explorer with `LEARN SELECTED`
- whether process-state wording feels understandable without prior Linux knowledge
- whether `/proc/<pid>/status` manual comparison works
- whether Safe Stop and Force Kill explanations are clear before confirmation
- whether confirmed SIGTERM/SIGKILL tests behave as expected
- any layout overlap, clipped lesson text, or confusing shortcut
- any mismatch against `/proc`, `free -h`, `df -h /`, or observed traffic

## Known alpha limitations

- Linux only
- English only
- root filesystem only
- network is aggregated across non-loopback interfaces
- thermal depends on sysfs exposure and is often unavailable in VMs
- Top CPU uses a short sample and total-machine share semantics
- Process Explorer does not yet have name search/filter or an all-process CPU column
- teaching lessons are static content enriched with live values; context-aware root-cause diagnosis is not implemented yet
- no lesson progress persistence
- thresholds are conservative heuristics, not failure diagnosis

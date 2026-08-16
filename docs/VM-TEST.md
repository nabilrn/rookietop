# RookieTop Alpha VM Test

This checklist qualifies `0.1.0-alpha.7` on a real Linux VM.

## 1. Record the environment

```sh
cat /etc/os-release
uname -a
nproc
free -h
df -h /
cat /proc/loadavg
```

## 2. Build and automated checks

```sh
git clone https://github.com/nabilrn/rookietop.git
cd rookietop
make clean check
```

Expected: strict compilation and all collector, process, teaching, diagnosis, and signal-safety tests pass.

## 3. Overview copy and diagnosis

```sh
./rookietop
```

Check that:

- the default screen uses plain language rather than teaching jargon
- `WHAT ROOKIETOP NOTICES` contains one short headline and one evidence-based detail
- a healthy machine says that nothing looks constrained rather than dumping technical definitions
- `[?] Explain` is visible but technical detail is not forced into the default view
- `p` still opens Process Explorer

## 4. CPU context

In another shell:

```sh
yes > /dev/null &
LOAD_PID=$!
```

Watch the overview for several refreshes, then stop it:

```sh
kill "$LOAD_PID"
```

Expected when CPU becomes high:

- RookieTop says the CPUs are busy
- if one visible process clearly contributes enough CPU, its name appears in the explanation
- wording says this is a short sample and does not claim a permanent root cause
- pressing `?` opens the lesson browser with CPU preselected

## 5. Memory context

Compare RookieTop with:

```sh
grep -E 'MemTotal|MemAvailable|MemFree|Cached' /proc/meminfo
```

If memory pressure is naturally high, the explanation may name the largest visible memory user. It must not claim that one process explains all system memory use.

Do not intentionally exhaust a small VM just to trigger the threshold.

## 6. Load context

Compare:

```sh
cat /proc/loadavg
nproc
```

Expected:

- high load with low CPU is described as possibly involving I/O or other waiting work
- high load with busy CPU is described as possible CPU queueing
- RookieTop never presents load as another CPU percentage

## 7. Process Explorer copy

Press `p`.

Expected:

- the header says to pick a process and see what it is doing
- the selected area is called `ABOUT THIS PROCESS`, not `LEARN SELECTED`
- state labels are readable (`Sleeping`, `Running`, `Waiting I/O`, etc.)
- technical terms such as RSS and procfs internals are not required to understand the default list
- the selected state gets one short plain-language explanation
- `[?] Explain`, `[Enter] Details`, `[k] Stop safely`, and `[K] Force kill` are discoverable

## 8. Process detail -> explanation progression

Select a stable process and press Enter.

The detail screen should show only useful basics first:

- PID
- state
- memory
- thread count
- command
- a short `WHAT THIS TELLS YOU` explanation

Press `?`.

Only then should RookieTop expose deeper Linux details such as:

- `/proc/<pid>/status`
- `/proc/<pid>/stat`
- `/proc/<pid>/cmdline`
- PID reuse and process start time
- `VmRSS`

Run the suggested command manually and compare the values:

```sh
cat /proc/<pid>/status
```

## 9. Safe Stop wording

Create a disposable process:

```sh
sleep 300 &
TEST_PID=$!
```

Find it and press `k`.

Expected copy:

- explains that RookieTop will ask the process to exit cleanly
- explains that it gets a chance to finish work and clean up
- shows `Linux signal: SIGTERM` as secondary technical context
- confirms with wording similar to `Stop it` / `Keep running`

Confirm and verify the process exits.

## 10. Force Kill wording

Use only a disposable process that ignores SIGTERM:

```sh
python3 -c 'import signal,time; signal.signal(signal.SIGTERM, signal.SIG_IGN); time.sleep(300)' &
TEST_PID=$!
```

Try Safe Stop first, then uppercase `K`.

Expected:

- warning says the process ends immediately and cannot clean up first
- `SIGKILL` is shown as the Linux signal
- Force Kill remains a separate explicit confirmation
- no automatic escalation and no automatic sudo occurs

## 11. Lesson copy

Press `?` from overview and open CPU, memory, and load.

Each lesson should use this progression:

```text
ON YOUR MACHINE
WHAT IT MEANS
WHEN TO CARE
HOW LINUX SHOWS IT
TRY IT
```

The lesson should sound conversational while still naming the real Linux source.

## 12. Regression checks

Verify:

```sh
./rookietop --once
NO_COLOR=1 ./rookietop --once
df -h /
```

Also generate normal network traffic and confirm RX/TX still changes. Missing thermal zones must remain non-fatal.

## Report back

Record:

- distro and kernel
- `make clean check` result
- screenshot of the alpha.7 overview
- screenshot of Process Explorer and `ABOUT THIS PROCESS`
- screenshot of one `? Explain` process view
- whether CPU diagnosis names the `yes` process when load is generated
- whether the copy feels understandable before seeing Linux terminology
- any layout overlap or clipped text
- any metric mismatch against `/proc`, `free -h`, or `df -h /`

## Known alpha limitations

- Linux only
- English only
- root filesystem only
- network is aggregated across non-loopback interfaces
- thermal depends on sysfs exposure and is often unavailable in VMs
- contextual diagnosis uses short live samples and conservative heuristics; it is not a root-cause oracle
- Process Explorer does not yet have name search/filter or an all-process CPU column
- no guided-lab completion state
- no final dedicated TUI mockup implementation yet

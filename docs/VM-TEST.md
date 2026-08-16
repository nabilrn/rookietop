# RookieTop Alpha VM Test

This checklist qualifies `0.1.0-alpha.8` on a real Linux VM.

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

Expected: strict compilation and all collector, process, search, teaching, diagnosis, and signal-safety tests pass.

## 3. Short runtime stress smoke

```sh
make stress
```

Expected:

- Process Explorer remains alive while short-lived processes churn
- CPU/memory sort-key probes remain responsive
- RSS stays bounded from the warm baseline
- average RookieTop CPU does not run away
- `q` exits promptly

For longer qualification, use the same harness:

```sh
ROOKIETOP_STRESS_SECONDS=1800 make stress
ROOKIETOP_STRESS_SECONDS=7200 make stress
ROOKIETOP_STRESS_SECONDS=28800 make stress
```

See `docs/PERFORMANCE.md` for the soak-test gate.

## 4. Overview copy and diagnosis

```sh
./rookietop
```

Check that:

- the default screen uses plain language rather than teaching jargon
- `WHAT ROOKIETOP NOTICES` contains one short headline and one evidence-based detail
- a healthy machine says that nothing looks constrained rather than dumping technical definitions
- `[?] Explain` is visible but technical detail is not forced into the default view
- `p` still opens Process Explorer

## 5. CPU context

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

## 6. Process Explorer search and CPU sorting

Press `p`.

Expected:

- the table shows PID, whole-machine CPU share, memory, readable state, threads, and name
- `c` sorts by CPU, `m` by memory, `p` by PID, and `n` by name
- `/` opens a simple case-insensitive name/PID search
- clearing the search restores the full list
- selection remains on the same `(PID,starttime)` across refresh/sort when that process still exists
- CPU percentages are whole-machine shares, not per-core percentages

Run a temporary CPU workload:

```sh
yes > /dev/null &
LOAD_PID=$!
```

Sort by CPU and confirm `yes` becomes visible near the top. Search for `yes` or `$LOAD_PID`, then stop it when finished.

## 7. Process Explorer copy

Expected:

- the selected area is called `ABOUT THIS PROCESS`
- state labels are readable (`Sleeping`, `Running`, `Waiting I/O`, etc.)
- technical terms such as RSS and procfs internals are not required to understand the default list
- `[?] Explain`, `[Enter] Details`, `[k] Stop safely`, and `[K] Force kill` are discoverable

## 8. Process detail -> explanation progression

Select a stable process and press Enter.

The detail screen should show useful basics first: PID, state, memory, thread count, command, and a short explanation.

Press `?`. Only then should RookieTop expose deeper Linux details such as `/proc/<pid>/status`, `/proc/<pid>/stat`, `/proc/<pid>/cmdline`, PID reuse, and `VmRSS`.

## 9. Safe Stop wording

Create a disposable process:

```sh
sleep 300 &
TEST_PID=$!
```

Find it and press `k`. Confirm that RookieTop explains SIGTERM as a clean-stop request and re-checks process identity before signalling.

## 10. Force Kill wording

Use only a disposable process that ignores SIGTERM:

```sh
python3 -c 'import signal,time; signal.signal(signal.SIGTERM, signal.SIG_IGN); time.sleep(300)' &
TEST_PID=$!
```

Try Safe Stop first, then uppercase `K`. Confirm that Force Kill remains a separate explicit SIGKILL warning with no automatic escalation or sudo.

## 11. Lesson copy

Press `?` from overview and open CPU, memory, and load. Each lesson should progress through live context, plain meaning, when to care, Linux source, and a small experiment.

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
- short `make stress` summary
- if performed, 30m/2h/8h stress summary
- screenshot of overview and Process Explorer
- whether CPU diagnosis names a generated `yes` workload
- whether search and sort feel immediate
- whether responsiveness gets worse over time
- any layout overlap or metric mismatch

## Known alpha limitations

- Linux only
- English only
- root filesystem only
- network is aggregated across non-loopback interfaces
- thermal depends on sysfs exposure and is often unavailable in VMs
- contextual diagnosis uses short live samples and conservative heuristics; it is not a root-cause oracle
- no guided-lab completion state
- no final dedicated TUI mockup implementation yet

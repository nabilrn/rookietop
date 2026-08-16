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

## 3. Overview regression

```sh
./rookietop
```

Check that:

- the default screen still uses plain language
- `WHAT ROOKIETOP NOTICES` contains one short evidence-based explanation
- `[?] Explain` remains visible
- `p` opens Process Explorer
- resize and terminal restoration still work

## 4. Process Explorer CPU column

Press `p`.

Expected:

- the table contains PID, CPU, MEMORY, STATE, THR, and NAME
- CPU values are percentages when sampling succeeds
- the screen explains that process CPU is a share of whole-machine CPU during the latest short sample
- a temporarily unavailable CPU sample displays `--` rather than breaking the process list

In another shell:

```sh
yes > /dev/null &
LOAD_PID=$!
```

Press `c` to sort by CPU. `yes` should move near the top while it is busy. Stop it afterwards:

```sh
kill "$LOAD_PID"
```

On a multi-core machine, remember that RookieTop process CPU is whole-machine share. One saturated single thread may therefore be well below 100%.

## 5. Sorting and stable selection

Select a stable process with Up/Down, then press each sort key:

```text
c  CPU
m  memory
p  PID
n  name
```

Expected: the selected process remains the same process after sorting when it still exists. RookieTop tracks selection by PID + process start time instead of silently moving the action target to whatever row lands at the old index.

## 6. Process search

Press `/`.

Check that the search screen clearly explains that a process name or PID can be entered.

Try:

- part of a process name such as `ngin`, `maria`, or `cloud`
- different capitalization such as `NGINX`
- part of a PID

Expected:

- search is case-insensitive for process names
- PID substring search works
- the Process Explorer header reports how many rows match
- no-match results explain how to change or clear the search
- pressing `/`, deleting the query, and pressing Enter restores all processes
- Esc cancels editing without replacing the active filter

## 7. Search + action safety

With a filter active, select a disposable test process:

```sh
sleep 300 &
TEST_PID=$!
```

Search for the PID, inspect it, then use `k`.

Expected: the same PID + process start-time safety guard applies from a filtered list. Search must not weaken signalling checks.

## 8. Process detail -> explanation progression

Select a stable process and press Enter.

The detail screen should show useful basics first:

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

## 9. Safe Stop and Force Kill regression

Create a disposable process:

```sh
sleep 300 &
TEST_PID=$!
```

Find it and press `k`. Confirm that SIGTERM remains the safe default and that RookieTop never auto-escalates.

For Force Kill, only use a disposable process that ignores SIGTERM:

```sh
python3 -c 'import signal,time; signal.signal(signal.SIGTERM, signal.SIG_IGN); time.sleep(300)' &
TEST_PID=$!
```

Try Safe Stop first, then uppercase `K`. SIGKILL must remain a separate explicit confirmation with no automatic sudo.

## 10. Teaching and diagnosis regression

Press `?` from overview and open CPU, memory, and load.

Each lesson should still use:

```text
ON YOUR MACHINE
WHAT IT MEANS
WHEN TO CARE
HOW LINUX SHOWS IT
TRY IT
```

Generate temporary CPU work and verify contextual diagnosis still names a visible contributor when the sample supports it.

## 11. Non-interactive regression

Verify:

```sh
./rookietop --once
NO_COLOR=1 ./rookietop --once
df -h /
```

Generate normal network traffic and confirm RX/TX still changes. Missing thermal zones must remain non-fatal.

## Report back

Record:

- distro and kernel
- `make clean check` result
- screenshot of alpha.8 Process Explorer
- whether CPU sorting brings `yes` near the top
- whether selection remains on the same PID when changing sort order
- whether name and PID search both work
- whether clearing search restores the full process list
- whether filtered process actions still target the selected process safely
- any layout overlap or confusing shortcut
- any metric mismatch against `/proc`, `free -h`, or `df -h /`

## Known alpha limitations

- Linux only
- English only
- root filesystem only
- network is aggregated across non-loopback interfaces
- thermal depends on sysfs exposure and is often unavailable in VMs
- contextual diagnosis uses short live samples and conservative heuristics; it is not a root-cause oracle
- process CPU uses a short whole-machine-share sample and can legitimately read 0.0% for idle processes
- no guided-lab completion state yet
- no final beginner UX qualification across multiple distros yet

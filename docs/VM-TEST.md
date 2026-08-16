# RookieTop Alpha VM Test

This checklist qualifies the current `0.1.0-alpha.5` Linux build on a real VM.

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

Expected: compilation succeeds with strict warnings and all tests pass. The process-list test also spawns a temporary child process, verifies the PID/start-time guard rejects a mismatched identity, then confirms a valid SIGTERM stops that child.

## 3. One-shot smoke test

```sh
./rookietop --once
```

Check that CPU, memory, swap, root disk, network, host context, Top CPU, and Top Memory render without root privileges.

## 4. Live terminal test

```sh
./rookietop
```

Expected:

- RookieTop uses the alternate full-screen terminal buffer
- the shell screen is restored after `Ctrl+C`
- terminal echo/canonical mode is restored after exit
- values refresh roughly once per second without terminal scrolling
- resizing the terminal updates the layout
- wide terminals show Top CPU and Top Memory side by side
- CPU/RAM activity history grows over time
- pressing `p` opens the process explorer

## 5. Process explorer

Inside RookieTop press `p`.

Check:

- Up/Down changes the selected row
- `m` sorts by memory
- `p` sorts by PID
- `n` sorts by process name
- Enter opens process details
- Esc returns to the previous screen
- process details show state, thread count, RSS, and command line

## 6. Graceful process stop

Create a disposable process in another shell:

```sh
sleep 300 &
TEST_PID=$!
echo "$TEST_PID"
```

In RookieTop's process explorer locate that PID, select it, press `k`, read the SIGTERM explanation, then press `y`.

Back in the shell:

```sh
wait "$TEST_PID"
```

Expected: the `sleep` process exits after confirmed SIGTERM. RookieTop must not ask for sudo or escalate to SIGKILL automatically.

## 7. Force-stop confirmation

Only use a disposable test process. Start one that intentionally ignores SIGTERM:

```sh
python3 -c 'import signal,time; signal.signal(signal.SIGTERM, signal.SIG_IGN); time.sleep(300)' &
TEST_PID=$!
echo "$TEST_PID"
```

First try `k` + `y`; the process should remain alive. Re-select it, press uppercase `K`, verify the warning explicitly says SIGKILL prevents cleanup, then press `y`.

Expected: force-stop requires its own explicit action and confirmation. RookieTop never performs this escalation automatically.

## 8. CPU and Top CPU behavior

In another shell:

```sh
yes > /dev/null &
LOAD_PID=$!
```

Watch RookieTop, then stop the load:

```sh
kill "$LOAD_PID"
```

Expected:

- aggregate CPU rises while `yes` runs and falls afterwards
- `yes` appears near the top of Top CPU
- per-process CPU is shown as a share of the whole machine, so on a multi-core VM one fully busy single-thread process may be well below 100%

## 9. Load-average behavior

Compare RookieTop with:

```sh
cat /proc/loadavg
nproc
```

Expected: RookieTop's 1/5/15-minute load values match `/proc/loadavg` closely. The health text should treat load as queue pressure relative to online CPUs, not as another CPU percentage.

## 10. Memory/process behavior

On a VM with enough free RAM, create a small temporary allocation:

```sh
python3 -c 'import time; x=bytearray(128*1024*1024); time.sleep(30)'
```

Expected: the Python process may appear in Top Memory. Do not use a large allocation on a small VM.

## 11. Network behavior

Generate a normal download or package update in another shell and watch the `down` rate. Then let traffic stop.

Expected: the rate rises during traffic and returns near zero. A disappearing or resetting interface should make only the network sample unavailable, not crash the monitor.

## 12. Thermal behavior

Check whether the VM exposes thermal zones:

```sh
ls /sys/class/thermal/thermal_zone*/temp 2>/dev/null
```

If none exist, RookieTop should show thermal as unavailable rather than failing. If zones exist, the dashboard shows the hottest valid exposed zone and its type.

## 13. Disk sanity check

Compare RookieTop root capacity with:

```sh
df -h /
```

Small differences in presentation are acceptable. Do not intentionally fill the VM disk just to trigger a warning.

## Report back

Record:

- distro and kernel
- `make clean check` result
- screenshot of the full-screen dashboard
- screenshot of the process explorer
- whether selection/sorting/detail navigation works
- whether confirmed SIGTERM stops the disposable `sleep` process
- whether SIGKILL requires a distinct uppercase `K` action and confirmation
- whether `yes` appears in Top CPU
- load values versus `/proc/loadavg`
- whether thermal is available or correctly shown as unavailable
- idle CPU/RSS of RookieTop if convenient
- any confusing wording or layout overlap
- any mismatch against `free -h`, `df -h /`, or observed traffic

## Known alpha limitations

- Linux only
- English only
- root filesystem only
- network is aggregated across non-loopback interfaces
- thermal depends on sysfs exposure and is often unavailable in VMs
- Top CPU uses a short sample and total-machine share semantics
- process explorer does not yet have name search/filter or an all-process CPU column
- thresholds are conservative heuristics, not failure diagnosis

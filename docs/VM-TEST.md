# RookieTop Alpha VM Test

This checklist qualifies the current `0.1.0-alpha.4` Linux build on a real VM.

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

Expected: compilation succeeds with strict warnings and all deterministic tests pass.

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
- values refresh roughly once per second without terminal scrolling
- resizing the terminal updates the layout
- wide terminals show Top CPU and Top Memory side by side
- CPU/RAM activity history grows over time

## 5. CPU and Top CPU behavior

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

## 6. Load-average behavior

Compare RookieTop with:

```sh
cat /proc/loadavg
nproc
```

Expected: RookieTop's 1/5/15-minute load values match `/proc/loadavg` closely. The health text should treat load as queue pressure relative to online CPUs, not as another CPU percentage.

## 7. Memory/process behavior

On a VM with enough free RAM, create a small temporary allocation:

```sh
python3 -c 'import time; x=bytearray(128*1024*1024); time.sleep(30)'
```

Expected: the Python process may appear in Top Memory. Do not use a large allocation on a small VM.

## 8. Network behavior

Generate a normal download or package update in another shell and watch the `down` rate. Then let traffic stop.

Expected: the rate rises during traffic and returns near zero. A disappearing or resetting interface should make only the network sample unavailable, not crash the monitor.

## 9. Thermal behavior

Check whether the VM exposes thermal zones:

```sh
ls /sys/class/thermal/thermal_zone*/temp 2>/dev/null
```

If none exist, RookieTop should show thermal as unavailable rather than failing. If zones exist, the dashboard shows the hottest valid exposed zone and its type.

## 10. Disk sanity check

Compare RookieTop root capacity with:

```sh
df -h /
```

Small differences in presentation are acceptable. Do not intentionally fill the VM disk just to trigger a warning.

## Report back

Record:

- distro and kernel
- `make clean check` result
- screenshot of the full-screen dashboard after at least 10 seconds
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
- thresholds are conservative heuristics, not failure diagnosis

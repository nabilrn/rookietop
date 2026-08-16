# RookieTop Alpha VM Test

This checklist is for the first manual Linux VM qualification of `0.1.0-alpha.1`.

## 1. Record the environment

```sh
cat /etc/os-release
uname -a
nproc
free -h
df -h /
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

Check that CPU, memory, swap, root disk, network, and Top Memory Processes render without root privileges.

## 4. Live terminal test

```sh
./rookietop
```

Expected in a normal interactive terminal:

- the screen refreshes roughly once per second
- values change without scrolling the terminal
- `Ctrl+C` exits normally
- the layout remains readable at a typical 80-column terminal width

## 5. CPU behavior

In another shell:

```sh
yes > /dev/null &
LOAD_PID=$!
```

Watch RookieTop, then stop the load:

```sh
kill "$LOAD_PID"
```

Expected: CPU rises while the process runs and falls afterwards. The explanation should not claim that a temporary spike is automatically a fault.

## 6. Memory/process behavior

On a VM with enough free RAM, create a small temporary allocation:

```sh
python3 -c 'import time; x=bytearray(128*1024*1024); time.sleep(30)'
```

Expected: the Python process may appear in Top Memory Processes. Do not use a large allocation on a small VM.

## 7. Network behavior

Generate a normal download or package update in another shell and watch the `down` rate. Then let traffic stop.

Expected: the rate rises during traffic and returns near zero. A disappearing or resetting interface should make only the network sample unavailable, not crash the monitor.

## 8. Disk sanity check

Compare RookieTop root capacity with:

```sh
df -h /
```

Small differences in presentation are acceptable. Do not intentionally fill the VM disk just to trigger a warning.

## Report back

Record:

- distro and kernel
- `make clean check` result
- one-shot output
- whether live refresh is stable
- idle CPU/RSS of RookieTop if convenient
- any confusing wording
- any mismatch against `free -h`, `df -h /`, or observed traffic

## Known alpha limitations

- Linux only
- English only
- root filesystem only
- network is aggregated across non-loopback interfaces
- Top Processes is memory-only; per-process CPU ranking is intentionally deferred
- no temperature or hardware sensor support yet
- thresholds are conservative heuristics, not failure diagnosis

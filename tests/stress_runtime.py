#!/usr/bin/env python3
"""Interactive RookieTop soak/stress test.

Runs RookieTop inside a pseudo-terminal, enters Process Explorer, creates steady
process churn, probes sort-key response latency, and samples the RookieTop
process's RSS/CPU usage from /proc.

The default duration is intentionally short enough for CI. Longer soak runs are
selected with ROOKIETOP_STRESS_SECONDS, for example 1800, 7200, or 28800.
"""

import fcntl
import os
import pty
import select
import signal
import struct
import subprocess
import sys
import termios
import threading
import time
from pathlib import Path

DURATION = float(os.environ.get("ROOKIETOP_STRESS_SECONDS", "15"))
WARMUP = float(os.environ.get("ROOKIETOP_STRESS_WARMUP_SECONDS", "2"))
MAX_RSS_GROWTH_KIB = int(os.environ.get("ROOKIETOP_STRESS_MAX_RSS_GROWTH_KIB", "8192"))
MAX_INPUT_LATENCY = float(os.environ.get("ROOKIETOP_STRESS_MAX_INPUT_LATENCY_SECONDS", "2.5"))
MAX_AVG_CPU_PERCENT = float(os.environ.get("ROOKIETOP_STRESS_MAX_AVG_CPU_PERCENT", "50"))
CHURN_INTERVAL = float(os.environ.get("ROOKIETOP_STRESS_CHURN_INTERVAL_SECONDS", "0.10"))

ROOT = Path(__file__).resolve().parent.parent
BINARY = ROOT / "rookietop"


def fail(message: str) -> None:
    print(f"stress: FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def set_winsize(fd: int, rows: int = 40, cols: int = 120) -> None:
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))


def read_chunk(master: int, timeout: float = 0.05) -> bytes:
    readable, _, _ = select.select([master], [], [], timeout)
    if not readable:
        return b""
    try:
        return os.read(master, 65536)
    except BlockingIOError:
        return b""
    except OSError:
        return b""


def wait_for(master: int, needle: bytes, timeout: float) -> float:
    started = time.monotonic()
    seen = bytearray()
    while time.monotonic() - started < timeout:
        chunk = read_chunk(master)
        if chunk:
            seen.extend(chunk)
            if len(seen) > 262144:
                del seen[:-131072]
            if needle in seen:
                return time.monotonic() - started
    tail = bytes(seen[-2000:]).decode("utf-8", "replace")
    fail(f"did not observe {needle!r} within {timeout:.1f}s; output tail:\n{tail}")
    return 0.0


def read_rss_kib(pid: int) -> int:
    with open(f"/proc/{pid}/status", "r", encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("VmRSS:"):
                return int(line.split()[1])
    raise RuntimeError("VmRSS missing")


def read_cpu_ticks(pid: int) -> int:
    text = Path(f"/proc/{pid}/stat").read_text(encoding="utf-8")
    close = text.rfind(")")
    if close < 0:
        raise RuntimeError("malformed /proc stat")
    fields = text[close + 2 :].split()
    return int(fields[11]) + int(fields[12])


def churn(stop: threading.Event) -> None:
    while not stop.is_set():
        try:
            child = subprocess.Popen(
                ["/bin/sh", "-c", "sleep 0.03"],
                stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            child.wait(timeout=2)
        except (OSError, subprocess.SubprocessError):
            return
        stop.wait(CHURN_INTERVAL)


def terminate_child(proc: subprocess.Popen[bytes]) -> None:
    if proc.poll() is not None:
        return
    try:
        proc.send_signal(signal.SIGTERM)
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=2)


def main() -> int:
    if not BINARY.exists():
        fail("./rookietop does not exist; build it first")
    if DURATION < 8:
        fail("ROOKIETOP_STRESS_SECONDS must be at least 8 seconds")

    master, slave = pty.openpty()
    set_winsize(slave)
    os.set_blocking(master, False)

    proc = subprocess.Popen(
        [str(BINARY)],
        cwd=ROOT,
        stdin=slave,
        stdout=slave,
        stderr=slave,
        close_fds=True,
        start_new_session=True,
    )
    os.close(slave)

    stop_churn = threading.Event()
    churn_thread = threading.Thread(target=churn, args=(stop_churn,), daemon=True)

    try:
        wait_for(master, b"RookieTop", 5.0)
        os.write(master, b"p")
        wait_for(master, b"PROCESS EXPLORER", 5.0)

        warmup_deadline = time.monotonic() + WARMUP
        while time.monotonic() < warmup_deadline:
            read_chunk(master, 0.05)

        if proc.poll() is not None:
            fail(f"rookietop exited during warmup with status {proc.returncode}")

        rss_start = read_rss_kib(proc.pid)
        rss_samples = [rss_start]
        tick_start = read_cpu_ticks(proc.pid)
        clock_start = time.monotonic()
        hz = os.sysconf(os.sysconf_names["SC_CLK_TCK"])

        churn_thread.start()

        latencies = []
        probe_cpu = True
        next_probe = time.monotonic() + 1.0
        next_sample = time.monotonic() + 1.0
        deadline = time.monotonic() + DURATION

        while time.monotonic() < deadline:
            read_chunk(master, 0.05)
            now = time.monotonic()

            if proc.poll() is not None:
                fail(f"rookietop exited early with status {proc.returncode}")

            if now >= next_sample:
                rss_samples.append(read_rss_kib(proc.pid))
                next_sample += 1.0

            if now >= next_probe:
                key = b"c" if probe_cpu else b"m"
                expected = b"sorted by CPU" if probe_cpu else b"sorted by memory"
                os.write(master, key)
                latency = wait_for(master, expected, MAX_INPUT_LATENCY + 1.0)
                latencies.append(latency)
                probe_cpu = not probe_cpu
                next_probe = time.monotonic() + 2.0

        rss_end = read_rss_kib(proc.pid)
        rss_samples.append(rss_end)
        tick_end = read_cpu_ticks(proc.pid)
        elapsed = time.monotonic() - clock_start
        cpu_seconds = (tick_end - tick_start) / float(hz)
        avg_cpu = cpu_seconds / elapsed * 100.0 if elapsed > 0.0 else 0.0
        rss_peak = max(rss_samples)
        rss_growth = rss_end - rss_start
        peak_growth = rss_peak - rss_start
        max_latency = max(latencies) if latencies else 0.0

        os.write(master, b"q")
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            fail("q did not exit RookieTop within 3 seconds")

        print(
            "stress: PASS candidate "
            f"duration={elapsed:.1f}s samples={len(rss_samples)} "
            f"rss_start={rss_start}KiB rss_end={rss_end}KiB "
            f"rss_peak={rss_peak}KiB rss_growth={rss_growth:+d}KiB "
            f"peak_growth={peak_growth:+d}KiB avg_cpu={avg_cpu:.2f}% "
            f"max_input_latency={max_latency:.3f}s probes={len(latencies)}"
        )

        if peak_growth > MAX_RSS_GROWTH_KIB:
            fail(
                f"RSS grew by {peak_growth} KiB from warm baseline; "
                f"limit is {MAX_RSS_GROWTH_KIB} KiB"
            )
        if max_latency > MAX_INPUT_LATENCY:
            fail(
                f"input response latency reached {max_latency:.3f}s; "
                f"limit is {MAX_INPUT_LATENCY:.3f}s"
            )
        if avg_cpu > MAX_AVG_CPU_PERCENT:
            fail(
                f"average RookieTop CPU was {avg_cpu:.2f}%; "
                f"limit is {MAX_AVG_CPU_PERCENT:.2f}%"
            )

        print("stress: PASS")
        return 0
    finally:
        stop_churn.set()
        if churn_thread.is_alive():
            churn_thread.join(timeout=2)
        terminate_child(proc)
        try:
            os.close(master)
        except OSError:
            pass


if __name__ == "__main__":
    raise SystemExit(main())

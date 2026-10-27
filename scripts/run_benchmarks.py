#!/usr/bin/env python3
"""Run correctness checks and export repeatable barrier measurements (stdlib only)."""

import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
import statistics
import subprocess
import sys
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
NAMES = {"SenseReversingBarrier", "SenseReversing (with backoff)",
         "StdBarrierWrapper (baseline)"}
FIELDS = ["Run", "Threads", "Barrier", "Episodes", "Seconds", "EpisodesPerSecond"]
ROW = re.compile(
    r"^(.+?)\s*\| Threads:\s*(\d+)\s*\| Episodes:\s*(\d+)"
    r"\s*\| Time:\s*([\d.]+) s\s*\| Rate:\s*([\d.]+) ep/s$"
)


def positive(value):
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("Expected a positive integer")
    return number


def default_episodes(threads):
    if threads == 1:
        return 20_000_000
    if threads == 2:
        return 2_000_000
    return 200_000 if threads < 16 else 20_000


def parse_rows(output, run, threads, episodes):
    rows = []
    for line in output.splitlines():
        match = ROW.fullmatch(line.strip())
        if not match:
            continue
        name, n, e, seconds, rate = match.groups()
        if int(n) != threads or int(e) != episodes:
            raise ValueError("Benchmark returned unexpected thread/episode counts")
        if not all(math.isfinite(float(v)) and float(v) > 0 for v in (seconds, rate)):
            raise ValueError("Invalid timing; increase the episode count")
        rows.append(dict(zip(FIELDS, [run, int(n), name.strip(), int(e),
                                      float(seconds), float(rate)])))
    if len(rows) != 3 or {r["Barrier"] for r in rows} != NAMES:
        raise ValueError("Expected exactly one measurement for each of the three barriers")
    return rows


def write_summary(rows, path):
    fields = ["Threads", "Barrier", "Episodes", "Samples", "MedianEpisodesPerSecond",
              "MinEpisodesPerSecond", "MaxEpisodesPerSecond"]
    groups = {}
    for row in rows:
        key = row["Threads"], row["Barrier"], row["Episodes"]
        groups.setdefault(key, []).append(row["EpisodesPerSecond"])
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(fields)
        for (threads, name, episodes), rates in sorted(groups.items()):
            writer.writerow([threads, name, episodes, len(rates), statistics.median(rates),
                             min(rates), max(rates)])


def run_logged(command, log, timeout, cwd=ROOT):
    with log.open("a", encoding="utf-8") as stream:
        stream.write("\nCOMMAND: " + json.dumps([str(x) for x in command]) + "\n")
        try:
            result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True,
                                    encoding="utf-8", errors="replace", timeout=timeout)
        except subprocess.TimeoutExpired as error:
            partial = error.stdout or b""
            stream.write(partial.decode("utf-8", "replace") if isinstance(partial, bytes) else partial)
            stream.write("\nTIMEOUT\n")
            raise RuntimeError(f"Timeout; see {log}") from error
        stream.write(result.stdout)
        if result.returncode:
            raise RuntimeError(f"Command failed ({result.returncode}); see {log}")
        return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--machine", required=True, help="Short name, e.g. khoi-mac or tri-windows")
    parser.add_argument("--physical-cores", type=positive, required=True)
    parser.add_argument("--power-mode", required=True, help="Power source and performance mode")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--repeats", type=positive, default=5)
    parser.add_argument("--threads", type=positive, nargs="+", help="Default: 1 2 4 8 16 plus physical/logical cores")
    parser.add_argument("--episodes", type=positive, nargs="+", help="One episode count per --threads entry")
    parser.add_argument("--timeout", type=positive, default=300, help="Seconds allowed per benchmark invocation")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", args.machine):
        parser.error("--machine must contain only letters, digits, underscores or hyphens")
    if not args.power_mode.strip():
        parser.error("--power-mode cannot be empty")
    if args.episodes and (not args.threads or len(args.episodes) != len(args.threads)):
        parser.error("Provide one --episodes value per --threads entry")
    if args.threads and len(set(args.threads)) != len(args.threads):
        parser.error("--threads must not contain duplicates")
    logical = os.cpu_count()
    threads = args.threads or sorted({1, 2, 4, 8, 16, args.physical_cores, logical or 1})
    episodes = args.episodes or [default_episodes(n) for n in threads]
    build = args.build_dir.resolve()
    info = build / "build-info-Release.txt"
    suffix = ".exe" if os.name == "nt" else ""
    candidates = [build / "bin" / "Release" / ("bench_barriers" + suffix),
                  build / "bin" / ("bench_barriers" + suffix)]
    executable = next((p for p in candidates if p.is_file()), None)
    ctest = shutil.which("ctest")
    if executable is None or not info.is_file():
        parser.error("Build this project in Release first; see README.md")
    if not ctest:
        parser.error("ctest was not found; add the CMake bin directory to PATH")

    stamp = datetime.now().astimezone()
    output = ROOT / "results" / f"{args.machine}-{stamp.strftime('%Y%m%d-%H%M%S-%f')}"
    output.mkdir(parents=True)
    metadata = {
        "status": "running", "started": stamp.isoformat(), "machine": args.machine,
        "platform": platform.platform(), "cpu": platform.processor(),
        "physical_cores_reported_by_user": args.physical_cores, "logical_processors": logical,
        "power_mode_reported_by_user": args.power_mode, "python": sys.version,
        "warmup_phases": 1000, "repeats": args.repeats,
        "plan": dict(zip(threads, episodes)), "build": info.read_text(encoding="utf-8"),
        "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "source_sha256": {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                          for folder in ("include", "benchmarks", "tests")
                          for p in sorted((ROOT / folder).rglob("*")) if p.is_file()},
    }
    metadata_path = output / "environment.json"

    def save_metadata():
        metadata_path.write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8")

    save_metadata()
    print(f"Results: {output}", flush=True)
    try:
        if os.name == "nt":
            hardware = ["powershell", "-NoProfile", "-Command",
                        "Get-CimInstance Win32_Processor | Format-List Name,NumberOfCores,NumberOfLogicalProcessors; "
                        "Get-CimInstance Win32_ComputerSystem | Format-List Manufacturer,Model,TotalPhysicalMemory"]
        elif sys.platform == "darwin":
            hardware = ["sysctl", "machdep.cpu.brand_string", "hw.model", "hw.memsize",
                        "hw.physicalcpu", "hw.logicalcpu"]
        else:
            hardware = ["lscpu"]
        try:
            run_logged(hardware, output / "hardware.txt", 30)
        except (OSError, RuntimeError) as error:
            with (output / "hardware.txt").open("a", encoding="utf-8") as stream:
                stream.write(str(error) + "\n")
            print("Could not collect all hardware details; add CPU model and RAM manually.", flush=True)
        print("Running CTest before measuring...", flush=True)
        run_logged([ctest, "--test-dir", str(build), "-C", "Release", "--output-on-failure", "-V"],
                   output / "correctness-release.log", 180)
        rows = []
        with (output / "benchmark-runs.csv").open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS)
            writer.writeheader()
            for run in range(1, args.repeats + 1):
                for n, e in zip(threads, episodes):
                    print(f"Run {run}/{args.repeats}: N={n}, E={e}", flush=True)
                    text = run_logged([str(executable), "--threads", str(n), "--episodes", str(e),
                                       "--barrier", "all"], output / "benchmark-runs.txt", args.timeout)
                    batch = parse_rows(text, run, n, e)
                    writer.writerows(batch)
                    stream.flush()
                    rows.extend(batch)
                    if any(row["Seconds"] < 0.1 for row in batch):
                        print("Short sample (<0.1 s): increase E for this N for ALL three barriers.", flush=True)
        if hashlib.sha256(executable.read_bytes()).hexdigest() != metadata["executable_sha256"]:
            raise RuntimeError("Executable changed during measurement; discard this run")
        write_summary(rows, output / "benchmark-summary.csv")
        metadata["status"] = "complete"
        metadata["measurements"] = len(rows)
    except Exception as error:
        metadata["status"] = "failed"
        metadata["error"] = str(error)
        raise
    finally:
        metadata["finished"] = datetime.now().astimezone().isoformat()
        save_metadata()
    print(f"Complete: {len(rows)} measurements. Send the whole folder: {output}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        sys.exit(str(error))

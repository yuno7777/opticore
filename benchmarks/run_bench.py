# OptiCore benchmark harness.
#
# For every instance in instances.toml (committed, deterministic scenarios):
#   1. ensure the .ocx scenario file exists (generated once via the CLI)
#   2. run the chronos CPU solver with the configured time budget
#   3. run the OR-Tools baseline with the same time budget
#   4. validate both solutions with the independent C++ validator
#   5. compute gaps against the best valid objective found in the run
#
# Outputs (never overwrite earlier runs):
#   benchmarks/results/run-<UTC-TIMESTAMP>.csv   machine-readable rows
#   benchmarks/results/run-<UTC-TIMESTAMP>.md    human summary
from __future__ import annotations

import csv
import datetime
import io
import pathlib
import platform
import subprocess
import sys
import time
import tomllib

ROOT = pathlib.Path(__file__).resolve().parents[1]
CLI = ROOT / "build" / "opticore.exe"
BASELINE = ROOT / "solvers" / "baseline" / "or_tools_baseline.py"
INSTANCES_PATH = ROOT / "benchmarks" / "instances.toml"
OUT_DIR = ROOT / "benchmarks" / "results"

CSV_COLUMNS = [
    "instance", "solver", "status", "wall_s", "total_cost", "travel_time_min",
    "travel_dist_km", "vehicles_used", "lateness_min", "served", "unserved",
    "validation", "violations", "gap_pct",
]


def machine_info() -> str:
    cpu = platform.processor()
    if not cpu:
        cpu = platform.machine()
    return (f"python {sys.version.split()[0]}, {platform.platform()}, "
            f"cpu='{cpu}'")


def parse_solve_output(text: str) -> dict:
    fields = {}
    for token in text.replace("\n", " ").split(" "):
        if "=" not in token:
            continue
        k, v = token.split("=", 1)
        k = k.strip()
        if not (k and v):
            continue
        try:
            fields[k] = float(v) if "." in v else int(v)
        except ValueError:
            fields[k] = v
    return fields


def run_solver(scenario: pathlib.Path, budget: float, seed: int,
               tmp_solution: pathlib.Path) -> tuple[dict, float]:
    t0 = time.perf_counter()
    r = subprocess.run(
        [str(CLI), "solve", "--scenario", str(scenario),
         "--solution", str(tmp_solution), "--seed", str(seed),
         "--time-budget-s", str(budget)],
        capture_output=True, text=True, timeout=budget + 120)
    wall = time.perf_counter() - t0
    if r.returncode != 0:
        return {"solver_error": r.stdout + r.stderr, "status": "solver_error"}, wall
    return parse_solve_output(r.stdout), wall


def run_and_validate(name: str, scenario: pathlib.Path, tmp_solution: pathlib.Path,
                     seed: int, cpu_budget: float, ot_budget: float) -> list[dict]:
    rows = []

    def validate(solution: pathlib.Path) -> dict:
        r = subprocess.run(
            [str(CLI), "validate", "--scenario", str(scenario),
             "--solution", str(solution)], capture_output=True, text=True)
        violations = sum(1 for line in r.stdout.splitlines()
                         if line.startswith("VIOLATION"))
        code = r.returncode
        if code == 2:
            status = "invalid"
        else:
            # pull the validation line: "validate: OK recomputed total_cost=..."
            status = "ok" if code == 0 else f"error_{code}"
        return {"status": status, "violations": violations, "out": r.stdout}

    # ---- chronos CPU ----
    f, wall = run_solver(scenario, cpu_budget, seed, tmp_solution)
    v = validate(tmp_solution)
    f_solved = not f.get("status", "").startswith("solver_error")
    row = {
        "instance": name,
        "solver": "chronos-cpu",
        "status": f.get("status", "error") if f_solved else "error",
        "wall_s": round(wall, 3),
        "total_cost": f.get("total_cost", ""),
        "travel_time_min": f.get("travel_time_min", ""),
        "travel_dist_km": f.get("travel_dist_km", ""),
        "vehicles_used": f.get("vehicles_used", ""),
        "lateness_min": f.get("lateness_min", ""),
        "served": f.get("served", ""),
        "unserved": f.get("unserved", ""),
        "validation": v["status"],
        "violations": v["violations"],
        "gap_pct": "",
    }
    rows.append(row)

    # ---- ortools baseline ----
    t0 = time.perf_counter()
    r = subprocess.run(
        [sys.executable, str(BASELINE), "--scenario", str(scenario),
         "--solution", str(tmp_solution), "--time-limit-s", str(ot_budget)],
        capture_output=True, text=True, timeout=ot_budget + 300)
    wall_ot = time.perf_counter() - t0
    if r.returncode != 0:
        rows.append({
            "instance": name, "solver": "ortools-9.15", "status": "error",
            "wall_s": round(wall_ot, 3), "validation": "error",
            "violations": "", "gap_pct": "",
        })
        return rows
    f2 = parse_solve_output(r.stdout)
    v2 = validate(tmp_solution)
    rows.append({
        "instance": name,
        "solver": "ortools-9.15",
        "status": f2.get("status", "error"),
        "wall_s": round(wall_ot, 3),
        "total_cost": f2.get("total_cost", ""),
        "travel_time_min": f2.get("travel_time_min", ""),
        "travel_dist_km": f2.get("travel_dist_km", ""),
        "vehicles_used": f2.get("vehicles_used", ""),
        "lateness_min": f2.get("lateness_min", ""),
        "served": f2.get("served", ""),
        "unserved": f2.get("unserved", ""),
        "validation": v2["status"],
        "violations": v2["violations"],
        "gap_pct": "",
    })
    return rows


def main() -> int:
    inst_def = tomllib.loads(INSTANCES_PATH.read_text(encoding="utf-8"))
    only = None
    if "--only" in sys.argv:
        only = sys.argv[sys.argv.index("--only") + 1]
    dry = "--dry-run" in sys.argv

    OUT_DIR.mkdir(exist_ok=True)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y%m%d-%H%M%S")
    csv_path = OUT_DIR / f"run-{stamp}.csv"
    md_path = OUT_DIR / f"run-{stamp}.md"

    buf = io.StringIO()
    writer = csv.DictWriter(buf, fieldnames=CSV_COLUMNS)
    writer.writeheader()

    all_rows = []
    for inst in inst_def.get("instances", []):
        name = inst["name"]
        if only and name != only:
            continue
        scenario = ROOT / "benchmarks" / "instances" / f"{name}.ocx"
        if not scenario.exists():
            cmd = [str(CLI), "gen", "--out", str(scenario), "--seed",
                   str(inst["seed"]), "--customers", str(inst["customers"]),
                   "--clusters", str(inst["clusters"]), "--vehicles",
                   str(inst["vehicles"]), "--name", name]
            print(f"[gen] {name}: {' '.join(cmd)}")
            if not dry:
                subprocess.run(cmd, check=True, capture_output=True)
        if dry:
            print(f"[dry] {name}: would run chronos {inst['cpu_budget_s']}s + "
                  f"ortools {inst['ortools_budget_s']}s")
            continue
        print(f"[run] {name} ...", flush=True)
        tmp = OUT_DIR / f".tmp-{name}.ocs"
        rows = run_and_validate(
            name, scenario, tmp, inst["seed"], inst["cpu_budget_s"],
            inst["ortools_budget_s"])
        tmp.unlink(missing_ok=True)
        all_rows.extend(rows)
        for rr in rows:
            writer.writerow(rr)
        buf.flush()

    # gaps: best valid cost per instance
    best_by_inst: dict[str, float] = {}
    for rr in all_rows:
        if rr.get("validation") == "ok" and isinstance(rr.get("total_cost"), (int, float)):
            cur = best_by_inst.setdefault(rr["instance"], float("inf"))
            best_by_inst[rr["instance"]] = min(cur, float(rr["total_cost"]))
    for rr in all_rows:
        if rr.get("validation") == "ok" and isinstance(rr.get("total_cost"), (int, float)) \
                and rr.get("total_cost") is not None:
            best = best_by_inst[rr["instance"]]
            if best > 0:
                rr["gap_pct"] = round(100.0 * (float(rr["total_cost"]) - best) / best, 3)

    if dry:
        return 0

    csv_path.write_text(buf.getvalue(), encoding="utf-8")

    lines = [
        f"# OptiCore benchmark run {stamp}",
        "",
        f"machine: {machine_info()}",
        f"opticore build: {'present' if CLI.exists() else 'MISSING'}",
        "",
        "| instance | solver | status | wall_s | total_cost | gap_pct | served | unserved | validation |",
        "|---|---|---|---|---|---|---|---|---|",
    ]
    for rr in all_rows:
        lines.append(
            f"| {rr['instance']} | {rr['solver']} | {rr.get('status', '')} | "
            f"{rr.get('wall_s', '')} | {rr.get('total_cost', '')} | "
            f"{rr.get('gap_pct', '')} | {rr.get('served', '')} | "
            f"{rr.get('unserved', '')} | {rr.get('validation', '')} |")
    lines.append("")
    lines.append(
        "Gap is relative to the best *valid* objective across solvers for the "
        "same instance within this run. Both solvers ran with the same time "
        "budget per instance.")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"results: {csv_path}")
    print(f"summary: {md_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

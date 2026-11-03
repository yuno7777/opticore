# OptiCore — Urban Logistics Optimization Engine (CPU edition)

OptiCore is a high-performance engine for **capacitated vehicle routing with
time windows (CVRPTW)**. It defines the optimization problem formally,
solves it with an independently-validated custom C++ solver, compares it to
a Google OR-Tools reference baseline on deterministic benchmark instances,
and documents every result — measured, never invented.

CUDA GPU acceleration is a documented *optional* build (see
`docs/CUDA_DESIGN.md`): this machine has no CUDA toolchain installed, so all
shipped results are CPU-only and honestly labeled as such.

## Quickstart

```powershell
# toolchain: MSYS2 UCRT64 (gcc), cran-winget CMake + Ninja on PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# unit + solver tests (18 total)
ctest --test-dir build --output-on-failure

# generate a deterministic scenario, solve it, validate it
build/opticore.exe gen --out examples/tiny-demo.ocx --seed 7 --customers 8 --vehicles 3 --name tiny-demo
build/opticore.exe solve --scenario examples/tiny-demo.ocx --solution examples/tiny-demo-solution.ocs --seed 7
build/opticore.exe validate --scenario examples/tiny-demo.ocx --solution examples/tiny-demo-solution.ocs

# OR-Tools reference baseline (pip install ortools)
python solvers/baseline/or_tools_baseline.py --scenario examples/tiny-demo.ocx --solution examples/tiny-demo-ortools.ocs --time-limit-s 5
build/opticore.exe validate --scenario examples/tiny-demo.ocx --solution examples/tiny-demo-ortools.ocs

# full benchmark suite (committed instances, results CSV + markdown)
python benchmarks/run_bench.py
```

## Repository layout

```text
core/          C++ domain model: scenario/solution formats, planner, objective, validator
simulation/    deterministic synthetic city scenario generator
solvers/cpu/   custom CPU solver "chronos" (savings + local search + seeded ILS)
solvers/baseline/  OR-Tools 9.15 reference baseline (Python)
solvers/cuda/  optional GPU experiment (documented, not built on this machine)
tools/         `opticore` CLI (gen / solve / validate / info / list-solvers)
benchmarks/    committed deterministic instances + harness (results CSV + md)
tests/         test harness, core/solver C++ tests, cross-language Python tests
docs/          engineering journal, formulations, ADRs, experiment records
```

## Status

See `docs/DOCUMENTING.md` (phase-by-phase milestone journal) and
`docs/EXPERIMENTS_AND_BENCHMARKS.md` (measured numbers). Headline:
on the committed benchmark suite the custom CPU solver matches the OR-Tools
baseline on small instances, is within ~0.3% on medium, and finishes in a
fraction of the shared time budget — every result independently validated.

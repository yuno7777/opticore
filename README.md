# OptiCore

A high-performance C++20 optimization engine for the Capacitated Vehicle Routing Problem with Time Windows (CVRPTW).

## Overview

OptiCore is an operations research engine for routing delivery fleets under vehicle capacity, maximum shift duration, and customer time window constraints. The engine minimizes a weighted objective across transit duration, road distance, vehicle fixed deployment costs, and customer lateness penalties.

The system is designed around three architectural contracts:
- **Zero-dependency C++20 core**: Domain types, search algorithms, schedule simulation, and validation link exclusively against the C++ standard library.
- **Single-truth schedule simulation**: Arrival times, waiting periods, service initiation, lateness, and depot return legs are evaluated by a single canonical function shared across all solvers and validators.
- **Independent validation**: Solutions are evaluated by a 13-rule validator that recalculates feasibility and cost directly from raw scenario cost matrices.

## Benchmarks: Chronos vs Google OR-Tools

All benchmarks were executed on an Intel Core i7-13620H (10 cores, 16 threads, 16 GB RAM) running Windows 11. Solvers were allocated identical time budgets. Every solution was checked and confirmed compliant by the independent validator.

| Instance | Problem Dimension | Chronos Objective | OR-Tools Objective | Optimality Gap (%) | Chronos Time (s) | OR-Tools Time (s) | Runtime Ratio |
|:---|:---|:---|:---|:---|:---|:---|:---|
| tiny-8 | 8 customers, 3 vehicles | 412.420 | 412.420 | 0.000% | 0.06 | 5.15 | 85x faster |
| small-20 | 20 customers, 5 vehicles | 631.426 | 631.426 | 0.000% | 0.19 | 15.15 | 79x faster |
| medium-50 | 50 customers, 10 vehicles | 1127.684 | 1124.330 | 0.298% | 1.07 | 30.15 | 28x faster |
| probe-120 | 120 customers, 18 vehicles | Feasible (0 unserved) | - | - | 9.45 | - | - |

On small instances, Chronos reaches identical solutions to Google OR-Tools Guided Local Search. On the 50-customer instance, Chronos reaches within 0.298% of the OR-Tools objective in 1.07 seconds.

## Architecture

```
Scenario File (.ocx)
        |
        +----------------------------+
        |                            |
        v                            v
Chronos (C++20)              Google OR-Tools 9.15
- Infeasibility screening    - Path cheapest arc
- Clarke-Wright savings      - Guided local search
- Variable neighborhood descent
- Iterated local search (ILS)
        |                            |
        +----------------------------+
        |
        v
Solution File (.ocs)
        |
        v
Independent Validator (13 Rules)
- Capacity and shift duration
- Time windows and service start
- Customer coverage verification
- Matrix distance and time recalculation
```

### Chronos Solver Pipeline
1. **Pre-screening**: Filters customers with demands exceeding vehicle capacity or time windows unreachable from the depot.
2. **Constructive Phase**: Parallel Clarke-Wright savings with deterministic tie-breaking, followed by cheapest feasible insertion for unassigned stops.
3. **Local Search**: Variable neighborhood descent evaluating Relocate (intra- and inter-route single customer shifts), Swap (customer exchanges), and 2-Opt (sub-tour edge reversals).
4. **Iterated Local Search**: Seeded perturbation operator removing k stops (k between 2 and 12) followed by greedy re-insertion and local search descent.

### Independent Validator
The validator verifies solutions against raw scenario matrices without relying on solver metadata:
- `V01-V03`: Node bounds, non-empty routes, depot start and end coordinates.
- `V04`: Customer coverage (every customer served once or explicitly listed with reason).
- `V05`: Vehicle capacity bounds (sum of demands <= Q).
- `V06`: Route duration limits (return time - start time <= L).
- `V07-V08`: Time window conformance (service start <= late window bound in hard mode).
- `V09`: Vehicle availability count (used vehicles <= K).
- `V10-V12`: Numerical consistency of reported travel time, distance, and objective cost within 1e-6.
- `V13`: Guard against empty solutions claiming feasibility.

### GPU Workload Design (CUDA)
The GPU implementation is structured for batch candidate route evaluation:
- Structure-of-Arrays (SoA) memory layout for candidate route offsets and stop sequences.
- Read-only matrix caching using `__ldg` global memory loads.
- Documented as an optional build path requiring MSVC and CUDA Toolkit <= 13.4.

## Quickstart

### Prerequisites
- C++ compiler supporting C++20 (GCC 12+, Clang 15+, MSVC 2022)
- CMake 3.20+ and Ninja
- Python 3.10+ with ortools (`pip install ortools`) for baseline comparison

### Build
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run Tests
```bash
# C++ unit and solver tests (15 tests)
ctest --test-dir build --output-on-failure

# Cross-language baseline integration tests (3 tests)
python -m unittest discover -s tests/python
```

### Execute Scenario
```bash
# Generate synthetic scenario
./build/opticore.exe gen --out scenario.ocx --seed 42 --customers 20 --vehicles 5 --name demo

# Solve using Chronos
./build/opticore.exe solve --scenario scenario.ocx --solution solution.ocs --seed 42

# Validate solution
./build/opticore.exe validate --scenario scenario.ocx --solution solution.ocs
```

### Run Benchmarks
```bash
python benchmarks/run_bench.py
```

## Repository Structure

```text
core/           C++20 domain types, file parsers, route planner, validator
simulation/     Deterministic synthetic clustered city generator
solvers/
  cpu/          Chronos C++ solver
  baseline/     Google OR-Tools reference baseline
  cuda/         GPU batch candidate evaluation design
tools/          Unified CLI entry point (opticore-cli.cpp)
benchmarks/     Committed benchmark instances and execution harness
tests/          C++ test harness, unit tests, and Python test suite
docs/           Technical documentation suite and Architecture Decision Records
```

## Documentation

- [docs/DOCUMENTING.md](docs/DOCUMENTING.md): Engineering log, environment inventory, and verification records.
- [docs/PROBLEM_FORMULATION.md](docs/PROBLEM_FORMULATION.md): Formal mathematical definitions, decision variables, and objective terms.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): Subsystem boundaries, data flows, and scheduling interfaces.
- [docs/DATA_AND_CITY_MODEL.md](docs/DATA_AND_CITY_MODEL.md): Matrix representations, city synthesis, and file formats.
- [docs/ALGORITHMS.md](docs/ALGORITHMS.md): Algorithmic complexity, neighborhood operators, and baseline settings.
- [docs/CUDA_DESIGN.md](docs/CUDA_DESIGN.md): Memory layout, kernel contracts, and break-even crossover analysis.
- [docs/EXPERIMENTS_AND_BENCHMARKS.md](docs/EXPERIMENTS_AND_BENCHMARKS.md): Empirical verification logs and stress test measurements.
- [docs/IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md): Phased roadmap and milestone criteria.
- [docs/decisions/](docs/decisions/): Architecture Decision Records (ADR-001 to ADR-006).

## License

MIT License. See [LICENSE](LICENSE) for details.

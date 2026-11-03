# OptiCore ⚡

**A High-Performance C++20 Optimization Engine for Capacitated Vehicle Routing with Time Windows (CVRPTW)**

[![C++20](https://img.shields.io/badge/standard-C%2B%2B20-blue.svg?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/build-CMake%20%2B%20Ninja-064F8C.svg?logo=cmake)](https://cmake.org/)
[![Tests](https://img.shields.io/badge/tests-18%2F18%20passing-brightgreen.svg)]()
[![Baseline](https://img.shields.io/badge/baseline-Google%20OR--Tools%209.15-orange.svg)](https://developers.google.com/optimization)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

---

## 📌 Overview

**OptiCore** is an industrial-grade Operations Research engine designed to solve the **Capacitated Vehicle Routing Problem with Time Windows (CVRPTW)**. It delivers production-grade route plans that minimize travel duration, distance, fleet fixed costs, and customer delivery lateness under tight capacity and service constraints.

Unlike toy routing scripts, OptiCore is built on strict systems engineering principles:
- **Zero Third-Party Dependencies in C++ Core**: The solver, domain models, forward scheduler, and validator link strictly against the C++20 standard library.
- **Single-Truth Forward Scheduler**: Customer arrival, wait times, service windows, lateness penalties, and return legs are computed by a single, canonical scheduler shared across solvers and validators.
- **Independent 13-Rule Feasibility Validator**: Solutions are never accepted simply because a solver reports success; every route is independently recomputed and verified against raw scenario matrices.
- **Rigorously Benchmarked Against Google OR-Tools**: Measured against Google OR-Tools 9.15 on standardized instances with zero fabricated numbers.

```
                    ┌────────────────────────────┐
                    │   Scenario Generator /     │
                    │   Raw Road Matrix (.ocx)   │
                    └─────────────┬──────────────┘
                                  │
                  ┌───────────────┴───────────────┐
                  ▼                               ▼
       ┌─────────────────────┐        ┌─────────────────────┐
       │   OptiCore Chronos  │        │   Google OR-Tools   │
       │   Custom C++ Solver │        │  Reference Baseline │
       │  CW + LS + SeededILS│        │  Guided Local Search│
       └──────────┬──────────┘        └──────────┬──────────┘
                  │                               │
                  └───────────────┬───────────────┘
                                  ▼
                    ┌────────────────────────────┐
                    │   Standardized Solution    │
                    │      Output (.ocs)         │
                    └─────────────┬──────────────┘
                                  ▼
                    ┌────────────────────────────┐
                    │    Independent Validator   │
                    │    (13 Rule Invariants)    │
                    └────────────────────────────┘
```

---

## 📊 Benchmark Highlights: Chronos vs. Google OR-Tools

Tested under identical wall-clock budgets on an Intel Core i7-13620H (10C/16T, 16 GB RAM). Every result is checked and confirmed compliant by the independent validator.

| Instance | Problem Size | Chronos Cost | OR-Tools Cost | Optimality Gap (%) | Chronos Time | OR-Tools Time | Speedup |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **`tiny-8`** | 8 cust, 3 veh | **412.420** | **412.420** | **0.000%** | **0.06 s** | 5.15 s | **$85\times$** |
| **`small-20`** | 20 cust, 5 veh | **631.426** | **631.426** | **0.000%** | **0.19 s** | 15.15 s | **$79\times$** |
| **`medium-50`** | 50 cust, 10 veh | 1127.684 | **1124.330** | **0.298%** | **1.07 s** | 30.15 s | **$28\times$** |
| **`probe-120`** | 120 cust, 18 veh | **Feasible** | — | **0 unserved** | **9.45 s** | — | — |

> **Key Takeaway**: Chronos reaches exact parity with OR-Tools on small instances and stays within **0.3%** on 50 customers while executing in **~1 second** ($28\times$ faster).

---

## 🛠️ Architecture & Core Components

### 1. Chronos Metaheuristic Pipeline (`solvers/cpu/chronos.cpp`)
1. **Pre-Screening**: Identifies structurally impossible requests ($q_i > Q$ or unreachable windows) before initiating search.
2. **Clarke-Wright Savings Construction**: Builds initial route clusters using parallel savings with deterministic tie-breaking.
3. **Cheapest Feasible Insertion**: Greedily recovers any unassigned customer requests.
4. **Multi-Neighborhood Local Search**: Variable neighborhood descent exploring **Relocate** (inter/intra-route shifts), **Swap** (customer exchanges), and **2-Opt** (sub-tour edge reversals).
5. **Seeded Iterated Local Search (ILS)**: Perturbs incumbent routes via seeded customer removal ($k \in [2, 12]$) and re-insertion, accepting only non-degrading moves.

### 2. Independent Feasibility Validator (`core/src/validator.cpp`)
Re-simulates routes against raw distance/time matrices to assert 13 invariant rules:
- **`V01–V03`**: Node index validity, non-empty stop sequences, valid depot endpoints.
- **`V04`**: Complete customer coverage (every customer served exactly once or explicitly listed as unserved).
- **`V05`**: Hard vehicle capacity enforcement ($\sum q_i \le Q$).
- **`V06`**: Route duration shift limit enforcement ($ret - T_0 \le L$).
- **`V07–V08`**: Time window compliance (service initiation $b_i \le l_i$ in hard TW mode).
- **`V09`**: Homogeneous fleet availability ($|\{v : R_v \neq \emptyset\}| \le K$).
- **`V10–V12`**: Floating-point numeric consistency within $10^{-6}$ tolerance.
- **`V13`**: Rejection of empty/no-op solutions claiming feasibility.

### 3. GPU Acceleration Architecture (`solvers/cuda/` & `docs/CUDA_DESIGN.md`)
Designed for Structure-of-Arrays (SoA) candidate batch evaluations to accelerate population-based metaheuristics:
- 1 CUDA block per candidate route evaluation.
- Read-only matrix caching using `__ldg` global memory loads.
- Break-even crossover analysis for batches $> 512$ candidates.

---

## 🚀 Quickstart

### Prerequisites
- **C++ Compiler**: GCC $\ge 12$ (tested with GCC 16.2), Clang $\ge 15$, or MSVC 2022.
- **Build System**: CMake $\ge 3.20$ and Ninja.
- **Python** (for baseline & benchmark harness): Python $\ge 3.10$ with `ortools` (`pip install ortools`).

### 1. Build the Engine
```powershell
# Configure and compile with C++20 and optimizations
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 2. Run Test Suites (18 Tests)
```powershell
# C++ Unit & Solver Tests (15 tests)
ctest --test-dir build --output-on-failure

# Cross-Language Python Integration Tests (3 tests)
python -m unittest discover -s tests/python
```

### 3. Generate, Solve, and Validate a Scenario
```powershell
# Generate a clustered synthetic delivery scenario (20 customers, 5 vehicles)
./build/opticore.exe gen --out scenario.ocx --seed 42 --customers 20 --vehicles 5 --name demo-city

# Solve with Chronos C++ solver
./build/opticore.exe solve --scenario scenario.ocx --solution solution.ocs --seed 42

# Run independent validation
./build/opticore.exe validate --scenario scenario.ocx --solution solution.ocs
```

### 4. Run the Full Benchmark Suite
```powershell
python benchmarks/run_bench.py
```
*Executes all standardized instances, validates outputs, and writes timestamped results to `benchmarks/results/`.*

---

## 📂 Repository Structure

```text
opticore/
├── core/                # Domain models, formats (.ocx/.ocs), planner, objective, validator
├── simulation/          # Clustered Euclidean synthetic scenario generator
├── solvers/
│   ├── cpu/             # Chronos C++ solver (Clarke-Wright + LS + ILS)
│   ├── baseline/        # Google OR-Tools 9.15 reference baseline
│   └── cuda/            # GPU candidate evaluation kernel design & contracts
├── tools/               # opticore unified CLI executable
├── benchmarks/          # Standardized instances (tiny/small/medium) and benchmark harness
├── tests/               # Test assertions, C++ unit/solver tests, Python integration
└── docs/                # Full documentation suite (15 documents + 6 ADRs)
```

---

## 📚 Documentation Index

| Document | Description |
|:---|:---|
| 📖 [**`DOCUMENTING.md`**](docs/DOCUMENTING.md) | Single source of truth: engineering journal, phase status, environment audit, bug log. |
| 📐 [**`PROBLEM_FORMULATION.md`**](docs/PROBLEM_FORMULATION.md) | Mathematical specification: sets, decision variables, hard/soft constraints, objective weights. |
| 🏗️ [**`ARCHITECTURE.md`**](docs/ARCHITECTURE.md) | Subsystem boundaries, scheduling single-truth, data flow diagrams. |
| 🗺️ [**`DATA_AND_CITY_MODEL.md`**](docs/DATA_AND_CITY_MODEL.md) | Clustered city generator, matrix interfaces, and `.ocx`/`.ocs` grammar. |
| ⚡ [**`ALGORITHMS.md`**](docs/ALGORITHMS.md) | Algorithmic analysis, neighborhood operators, time complexity, and baseline setup. |
| 🚀 [**`CUDA_DESIGN.md`**](docs/CUDA_DESIGN.md) | SoA memory layouts, kernel specifications, and GPU break-even methodology. |
| 📈 [**`EXPERIMENTS_AND_BENCHMARKS.md`**](docs/EXPERIMENTS_AND_BENCHMARKS.md) | Verified empirical runs, hardware specs, and large-scale stress tests. |
| 📋 [**`IMPLEMENTATION_PLAN.md`**](docs/IMPLEMENTATION_PLAN.md) | Phased roadmap (Phases 0–9), dependencies, and verifiable exit criteria. |
| 🏛️ [**`docs/decisions/`**](docs/decisions/) | Architecture Decision Records (ADR-001 through ADR-006). |

---

## 📜 License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

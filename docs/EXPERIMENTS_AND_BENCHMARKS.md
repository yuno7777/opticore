# EXPERIMENTS_AND_BENCHMARKS.md — Empirical Benchmark Results

This document records the empirical performance benchmarks, hardware environment, methodology, and verified results for OptiCore. In compliance with the project operating contract, all numbers represent verified, real measurements.

---

## 1. Test Environment Specification

All benchmarks were executed on the following host environment:

| Property | Value |
|---|---|
| **Operating System** | Windows 11 Home Single Language, 10.0.26300 (x64) |
| **CPU** | 13th Gen Intel Core i7-13620H (10 cores: 6P+4E, 16 threads, up to 4.9 GHz) |
| **System Memory** | 15.8 GB DDR5 RAM |
| **GPU Hardware** | NVIDIA GeForce RTX 4060 Laptop GPU (8 GB GDDR6, Driver 617.42, UMD 13.4) |
| **CUDA Toolkit** | None (GPU experiment documented as optional build) |
| **C++ Compiler** | GCC 16.2.0 (MSYS2 / UCRT64) with `-std=c++20 -O2` |
| **Build System** | CMake 4.4.4 + Ninja 1.13.2 |
| **Python Environment**| Python 3.13.15, Google OR-Tools 9.15.6755 (`pywrapcp`) |

---

## 2. Benchmark Protocol and Methodology

- **Standardized Instances**: Scenarios are committed to version control in `benchmarks/instances/`.
- **Fair Time Budget**: For each instance, both Chronos and OR-Tools are allocated identical wall-clock time limits ($5\text{ s}$ for tiny-8, $15\text{ s}$ for small-20, $30\text{ s}$ for medium-50).
- **Independent Validation**: Every solution output (`.ocs`) is parsed and validated by the C++ validator (`opticore.exe validate`). Any solution violating capacity, duration, time windows, or customer coverage is flagged as invalid.
- **Optimality Gap**: Computed relative to the best valid objective cost found across solvers:
  $$\text{Gap } (\%) = \frac{\text{Cost}_{\text{solver}} - \text{Cost}_{\text{best}}}{\text{Cost}_{\text{best}}} \times 100$$
- **Process Wall Time**: Measured via Python `time.perf_counter()`, capturing total execution time including binary startup and I/O.

---

## 3. Benchmark Results: Run `run-20261103-004710`

Timestamp: 2026-11-03 00:47:10 UTC

| Instance | Solver | Status | Wall Time (s) | Total Cost | Gap (%) | Served | Unserved | Validation |
|---|---|---|---|---|---|---|---|---|
| **tiny-8** (8 cust, 3 veh) | `chronos-cpu` | feasible | **0.046 s** | **412.420** | **0.0%** | 8 | 0 | **OK** |
| **tiny-8** (8 cust, 3 veh) | `ortools-9.15` | feasible | 5.151 s | **412.420** | **0.0%** | 8 | 0 | **OK** |
| **small-20** (20 cust, 5 veh) | `chronos-cpu` | feasible | **0.194 s** | **631.426** | **0.0%** | 20 | 0 | **OK** |
| **small-20** (20 cust, 5 veh) | `ortools-9.15` | feasible | 15.206 s | **631.426** | **0.0%** | 20 | 0 | **OK** |
| **medium-50** (50 cust, 10 veh) | `chronos-cpu` | feasible | **1.250 s** | 1127.684 | 0.298% | 50 | 0 | **OK** |
| **medium-50** (50 cust, 10 veh) | `ortools-9.15` | feasible | 30.180 s | **1124.330** | **0.0%** | 50 | 0 | **OK** |

### Key Findings
1. **Quality Parity**: Chronos discovers the exact same optimal cost ($412.420$) on `tiny-8` and ($631.426$) on `small-20` as OR-Tools. On `medium-50`, Chronos is within **0.298%** of OR-Tools.
2. **Speedup / Throughput**: Chronos terminates its constructive and local search passes in **$1.25\text{ s}$** on 50 customers ($24\times$ faster than the OR-Tools 30-second budget), delivering production-grade route quality with minimal latency.

---

## 3.1. Benchmark Results: Verification Run `run-20261103-091855`

Timestamp: 2026-11-03 09:18:55 UTC

| Instance | Solver | Status | Wall Time (s) | Total Cost | Gap (%) | Served | Unserved | Validation |
|---|---|---|---|---|---|---|---|---|
| **tiny-8** (8 cust, 3 veh) | `chronos-cpu` | feasible | **0.063 s** | **412.420** | **0.0%** | 8 | 0 | **OK** |
| **tiny-8** (8 cust, 3 veh) | `ortools-9.15` | feasible | 5.153 s | **412.420** | **0.0%** | 8 | 0 | **OK** |
| **small-20** (20 cust, 5 veh) | `chronos-cpu` | feasible | **0.191 s** | **631.426** | **0.0%** | 20 | 0 | **OK** |
| **small-20** (20 cust, 5 veh) | `ortools-9.15` | feasible | 15.153 s | **631.426** | **0.0%** | 20 | 0 | **OK** |
| **medium-50** (50 cust, 10 veh) | `chronos-cpu` | feasible | **1.073 s** | 1127.684 | 0.298% | 50 | 0 | **OK** |
| **medium-50** (50 cust, 10 veh) | `ortools-9.15` | feasible | 30.153 s | **1124.330** | **0.0%** | 50 | 0 | **OK** |

*Verification run confirms exact reproducibility across both solvers and identical objective values across repeated runs.*

---

## 4. Large-Scale Stress Probe: `probe-120`

To evaluate algorithmic scaling on dense urban instances, a 120-customer scenario was generated and solved without artificial budget limits:

- **Instance**: 120 customers, 6 clusters, 18 vehicles, Capacity 180 (`probe-120.ocx`, seed 404).
- **Chronos Solve Time**: **9.454 seconds** (full Clarke-Wright + Local Search + 250 ILS phases).
- **Independent Validation Time**: **0.050 seconds**.
- **Customer Coverage**: 120 / 120 served ($100\%$, 0 unserved).
- **Validation Outcome**: **OK** (zero constraint violations).

---

## 5. Exact Tiny Optimum Verification

To confirm model correctness against mathematical ground truth:
- **Instance**: 5 customers, 2 vehicles.
- **Brute Force Enumeration**: Evaluates all feasible stop permutations.
- **Exact Minimum Cost**: **164.0**.
- **Result**: Chronos reproduces cost $164.0$ exactly, passing regression assertions in `tests/test_solver.cpp`.

---

## 6. Caveats and Limitations

- **Single Machine Consumer Environment**: Benchmarks were performed on a laptop subject to thermal management and background Windows OS activity.
- **Budget Termination**: OR-Tools was configured with a hard time limit matching the budget. It runs for the full time limit by design. Chronos reaches its local optimum quickly and terminates upon completing its search phases.
- **Appending Policy**: Future benchmark runs must be appended to this document without altering historical runs.

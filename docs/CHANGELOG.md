# CHANGELOG.md — Chronological Change Log

All notable changes to the OptiCore engine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] — 2026-11-03

### Added
- **Core Domain & Types (`core/`)**:
  - Strongly-typed domain structures for nodes, vehicles, demands, time windows, and matrices in `types.hpp`.
  - Single-truth forward route scheduler in `route_planner.cpp` enforcing arrival times, waiting, service initiation, and depot return.
  - Objective calculator with 5 configurable penalty weights (time, distance, vehicle fixed, lateness, unserved).
  - Independent 13-rule feasibility validator (`validator.cpp`) arbitrating solutions from raw matrices.
  - Frozen v1 text file formats (`.ocx` for scenarios, `.ocs` for solutions) with C++ parsers/formatters.
- **Simulation Engine (`simulation/`)**:
  - Clustered Euclidean synthetic city generator with seeded `mt19937_64` generator (`city_model.cpp`).
- **Custom CPU Solver: Chronos (`solvers/cpu/`)**:
  - Infeasibility pre-screening for oversized orders and unreachable windows.
  - Constructive Clarke-Wright savings with deterministic tie-breaking.
  - Greedy cheapest insertion for orphaned requests.
  - Multi-neighborhood local search: Relocate, Swap, and 2-Opt.
  - Seeded Iterated Local Search (ILS) with customer destroy/repair operators.
- **Google OR-Tools Baseline (`solvers/baseline/`)**:
  - External reference solver runner using Google OR-Tools 9.15 (`pywrapcp`).
  - Integer cost scaling, fixed vehicle penalties, and Guided Local Search metaheuristic.
- **Unified CLI (`tools/opticore-cli.cpp`)**:
  - Subcommands: `info`, `gen`, `solve`, `validate`, `list-solvers`.
- **Benchmark Suite (`benchmarks/`)**:
  - Committed instances: `tiny-8`, `small-20`, `medium-50`.
  - Automated benchmark harness (`run_bench.py`) producing CSV and Markdown summaries with optimality gap calculations.
- **Testing (`tests/`)**:
  - Lightweight C++ assertion harness (`tester.hpp`).
  - 9 core unit tests in `test_core.cpp`.
  - 6 solver correctness and tiny optimum tests in `test_solver.cpp`.
  - 3 cross-language Python baseline tests in `test_baseline.py`.
- **Architecture & Design Documentation (`docs/`)**:
  - Engineering journal (`DOCUMENTING.md`), formulation (`PROBLEM_FORMULATION.md`), architecture (`ARCHITECTURE.md`), implementation plan (`IMPLEMENTATION_PLAN.md`), data model (`DATA_AND_CITY_MODEL.md`), algorithms (`ALGORITHMS.md`), GPU design (`CUDA_DESIGN.md`), API spec (`API_SPECIFICATION.md`), database schema (`DATABASE_SCHEMA.md`), simulation spec (`SIMULATION_AND_SCENARIOS.md`), testing strategy (`TESTING_STRATEGY.md`), security (`SECURITY.md`), deployment (`DEPLOYMENT.md`), and benchmark records (`EXPERIMENTS_AND_BENCHMARKS.md`).
  - Architecture Decision Records ADR-001 through ADR-006 in `docs/decisions/`.

### Fixed
- Fixed scenario parser state machine to properly branch into matrix sections (`BEGIN_MATRIX_TIME_MIN`).
- Fixed scenario generator auto-capacity sizing from $0.9\times$ to $1.2\times$ average vehicle load to prevent structural fleet shortage.
- Fixed ILS monotonicity test semantics.
- Fixed unserved customer counting in brute-force tiny instance optimizer.

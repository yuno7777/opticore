# IMPLEMENTATION_PLAN.md — OptiCore Phased Execution Plan

This document outlines the phased roadmap for OptiCore, detailing prerequisites, dependencies, milestones, and verifiable exit criteria.

---

## Phase Matrix Overview

| Phase | Title | Focus Area | Status |
|---|---|---|---|
| **Phase 0** | Reconnaissance & Environment | Machine audit, toolchain inventory, feasibility probe | **Completed** |
| **Phase 1** | Problem Formalization & Specs | Mathematical formulation, units, architecture, ADRs | **Completed** |
| **Phase 2** | Domain Model & Validator | In-memory models, frozen text formats, independent validator | **Completed** |
| **Phase 3** | CPU Baseline Reference | OR-Tools 9.15 runner, format alignment, baseline acceptance | **Completed** |
| **Phase 4** | Custom C++ Solver | Chronos (CW savings + LS + ILS), determinism, tiny optima | **Completed** |
| **Phase 5** | Benchmark Harness | Fixed instances (tiny/small/medium), automated CSV/MD reporting | **Completed** |
| **Phase 6** | CUDA Acceleration | Kernel contracts, SoA memory design, optional build path | **Documented (Optional Build)** |
| **Phase 7** | City Network & Visualization | OSM road graph ingestion, route rendering (MapLibre/HTML) | **Planned** |
| **Phase 8** | Dynamic Logistics Extensions | Multi-depot, pickup & delivery, EV charging constraints | **Planned** |
| **Phase 9** | Hardening & Clean-Clone Release | Sanitizers, CI workflow, build audit, documentation pass | **Active / Near Completion** |

---

## Detailed Phase Breakdown

### Phase 0 — Reconnaissance & Environment
- **Objective**: Survey machine hardware, compiler capabilities, disk, memory, and libraries before writing code.
- **Dependencies**: Host system access.
- **Exit Criteria**: Environment inventory recorded in `DOCUMENTING.md`; successful compilation of C++20 smoke test; toolchain decision recorded (ADR-001).
- **Outcome**: Windows 11 Home, i7-13620H, 16GB RAM, RTX 4060 Laptop (driver 617.42, CUDA 13.4 UMD). GCC 16.2.0 (MSYS2 UCRT64), CMake 4.4.4, Ninja 1.13.2, Python 3.13.15, OR-Tools 9.15.6755. CUDA toolkit absent; user approved CPU-first with documented optional GPU track.

---

### Phase 1 — Problem Formalization & Specs
- **Objective**: Formalize the mathematical optimization problem, decision variables, hard/soft constraints, objective weights, and architecture.
- **Dependencies**: Phase 0.
- **Exit Criteria**: `PROBLEM_FORMULATION.md`, `ARCHITECTURE.md`, `DOCUMENTING.md`, and initial ADRs committed.
- **Outcome**: Complete CVRPTW formulation with single-depot, forward scheduling semantics, 5-term objective function, and strict units (km, minutes, load units).

---

### Phase 2 — Scenario Model & Independent Validator
- **Objective**: Implement deterministic scenario/solution I/O formats (`.ocx`, `.ocs`), single-truth route planner, and an independent validator.
- **Dependencies**: Phase 1.
- **Exit Criteria**: Independent validator correctly identifies valid routes and catches violations (capacity, duration, time windows, missing/duplicate stops, invalid node IDs). 9 unit tests pass.
- **Outcome**: `core/` library implemented with 13 distinct validation rule codes (`V01`–`V13`). Hand-crafted valid/invalid test cases verified in `oc_tests_core.exe`.

---

### Phase 3 — CPU Baseline (Google OR-Tools)
- **Objective**: Integrate a trusted, industry-standard reference solver to benchmark solution quality and run times.
- **Dependencies**: Phase 2.
- **Exit Criteria**: Baseline solver executes on `.ocx` instances, emits `.ocs` files, and passes all rules of the C++ independent validator.
- **Outcome**: `solvers/baseline/or_tools_baseline.py` created using OR-Tools 9.15 Routing library (`PATH_CHEAPEST_ARC` + `GUIDED_LOCAL_SEARCH`). Solutions independently validated.

---

### Phase 4 — Custom C++ Solver (Chronos)
- **Objective**: Develop a high-performance C++ heuristic/metaheuristic from scratch without third-party solver dependencies.
- **Dependencies**: Phase 2.
- **Exit Criteria**: Custom solver produces valid solutions; matches brute-force known optimum on tiny instance; demonstrates reproducible runs given fixed seeds; tests pass.
- **Outcome**: `solvers/cpu/chronos.cpp` implemented with customer screening, Clarke-Wright savings, relocate/swap/2-opt local search, and seeded ILS. Passes 6 solver unit tests in `oc_tests_solver.exe`. Matches tiny exact optimum ($164.0$).

---

### Phase 5 — Benchmark Harness & Standardized Instances
- **Objective**: Standardize test instances across sizes, establish automated comparison harness, and publish measured metrics.
- **Dependencies**: Phase 3, Phase 4.
- **Exit Criteria**: Single-command execution (`python benchmarks/run_bench.py`) producing timestamped CSV and Markdown summaries with zero fabricated data.
- **Outcome**: Benchmark instances committed (`tiny-8`, `small-20`, `medium-50`). Benchmark run `20261103-004710` executed. Chronos achieves 0.0% gap on tiny-8 and small-20, and 0.298% gap on medium-50, executing in $1.25\text{ s}$ vs. $30.18\text{ s}$ for OR-Tools.

---

### Phase 6 — CUDA Acceleration Experiment
- **Objective**: Accelerate compute-heavy candidate route evaluations on NVIDIA GPUs.
- **Dependencies**: Host MSVC compiler + CUDA Toolkit $\le 13.4$.
- **Exit Criteria**: Functional CUDA kernel for batch candidate evaluation; verified numerical equivalence against CPU reference; measured kernel-only and end-to-end time; break-even analysis reported.
- **Current Status**: Documented optional build (`docs/CUDA_DESIGN.md`). Blocked on local machine due to absence of MSVC and CUDA toolkit.

---

### Phase 7 — City Network & Visualization (Future)
- **Objective**: Ingest OpenStreetMap road graph extracts, derive non-Euclidean travel matrices, and render routes on an interactive map.
- **Dependencies**: Phase 5.
- **Exit Criteria**: Map visualization of optimized stop sequences with ODbL attribution.

---

### Phase 8 — Dynamic Logistics Extensions (Future)
- **Objective**: Extend model to multi-depot VRP, dynamic customer arrivals, vehicle battery/charging windows.
- **Dependencies**: Phase 5.
- **Exit Criteria**: Documented formulation extensions, baseline adaptors, and benchmark evaluations.

---

### Phase 9 — Hardening, Clean-Clone Build, and Release
- **Objective**: Ensure complete documentation integrity, clean build from fresh checkout, compiler warning hygiene (`-Wall -Wextra -Wpedantic`), and release audit.
- **Dependencies**: Phases 0–5.
- **Exit Criteria**: Clean build on fresh checkout; full test suite passing; zero undocumented features or fabricated results.
- **Current Status**: In progress / finalization.

# ARCHITECTURE.md — OptiCore Systems Architecture

This document describes the software architecture, module contracts, data flow, and CPU/GPU boundary design for OptiCore.

---

## 1. High-Level Design Principles

1. **Separation of Concerns**: Domain data representation, schedule computation, feasibility validation, and search algorithms reside in distinct modules with explicit interfaces.
2. **Single Truth for Scheduling**: Route schedule calculation (arrival times, waiting, service windows, departure, elapsed duration) is implemented once in `core/route_planner` and shared across solvers, objective calculators, and the independent validator. Solvers do not maintain diverging cost arithmetic.
3. **Independent Validation**: Solvers produce candidate solutions; acceptance is governed strictly by an independent validator that parses the solution and checks every constraint against raw scenario inputs.
4. **Reproducibility & Zero Dependency Core**: The C++ core relies exclusively on the C++20 standard library. No third-party binaries or libraries are linked into `opticore_core`. Scenarios and solutions use deterministic, text-based, human-readable file formats (`.ocx` and `.ocs`).
5. **Pluggable Baseline**: Google OR-Tools is integrated as an external reference baseline runner (via Python) that produces the standard `.ocs` format, ensuring fair, apples-to-apples validation and comparison.

---

## 2. Repository and Module Map

```text
opticore/
├── core/                           # C++20 core library (opticore_core)
│   ├── include/opticore/
│   │   ├── types.hpp               # Domain primitives, units, and enumerations
│   │   ├── scenario.hpp            # In-memory scenario model and .ocx I/O
│   │   ├── solution.hpp            # In-memory solution model and .ocs I/O
│   │   ├── route_planner.hpp       # Single-truth forward scheduling logic
│   │   ├── objective.hpp           # Objective breakdown and weighting
│   │   ├── validator.hpp           # 13-rule independent feasibility validator
│   │   ├── solver.hpp              # Abstract Solver interface
│   │   ├── rng.hpp                 # Seeded 64-bit Mersenne Twister wrapper
│   │   └── version.hpp             # Version and build constants
│   └── src/
│       ├── types.cpp
│       ├── scenario.cpp
│       ├── solution.cpp
│       ├── route_planner.cpp
│       ├── objective.cpp
│       └── validator.cpp
├── simulation/                     # Scenario synthesis
│   ├── city_model.hpp              # Clustered Euclidean synthetic city generator
│   └── city_model.cpp
├── solvers/                        # Solvers and baselines
│   ├── cpu/
│   │   ├── chronos.hpp             # Custom C++ heuristic/metaheuristic
│   │   └── chronos.cpp             # Savings + Local Search + Seeded ILS
│   ├── baseline/
│   │   └── or_tools_baseline.py    # Google OR-Tools 9.15 CVRPTW baseline
│   └── cuda/                       # GPU acceleration design and contracts
├── tools/
│   └── opticore-cli.cpp            # Unified CLI binary (opticore.exe)
├── benchmarks/                     # Reproducible benchmark suite
│   ├── instances/                  # Committed deterministic test instances
│   ├── results/                    # Timestamped benchmark CSVs and summaries
│   ├── instances.toml              # Scenario definitions
│   └── run_bench.py                # Automated benchmark harness
├── tests/                          # Automated test suites
│   ├── harness/tester.hpp          # Lightweight C++ test assertion framework
│   ├── test_core.cpp               # Core unit tests (types, parser, validator)
│   ├── test_solver.cpp             # Solver correctness and optimality tests
│   └── python/
│       └── test_baseline.py        # Cross-language baseline integration tests
├── docs/                           # Documentation and ADRs
│   ├── DOCUMENTING.md              # Engineering journal and single truth
│   ├── PROBLEM_FORMULATION.md      # Mathematical specification
│   ├── ARCHITECTURE.md             # This document
│   ├── ...
│   └── decisions/                  # Architecture Decision Records (ADRs)
└── CMakeLists.txt                  # Top-level build configuration
```

---

## 3. Data Flow and System Pipelines

```
[Scenario Generation / File Input]
             │
             ▼
     [.ocx Scenario File]
             │
             ├─────────────────────────────────────────┐
             ▼                                         ▼
   [opticore CLI / chronos]              [solvers/baseline/or_tools]
   - Parse Scenario                      - Parse Scenario
   - Screening & Clarke-Wright           - Initialize RoutingModel
   - Relocate / Swap / 2-Opt             - Guided Local Search
   - Seeded Iterated Local Search        - Export Route Sequences
             │                                         │
             ▼                                         ▼
   [.ocs Solution File]                  [.ocs Solution File]
             │                                         │
             └───────────────────┬─────────────────────┘
                                 │
                                 ▼
                     [Independent Validator]
                     - Raw matrix verification
                     - Customer coverage check
                     - Capacity check
                     - Time-window & duration check
                     - Objective re-computation
                                 │
                                 ▼
                    [Validation Report & Logs]
```

---

## 4. Subsystem Details

### 4.1. Core Domain Model (`core/`)

- **Coordinate & Network Model**: Locations are represented by integer IDs ($0 \dots N-1$, with $0$ designating the single depot). Travel times ($t_{ij}$, minutes) and travel distances ($d_{ij}$, kilometers) are provided via dense 2D matrices. Asymmetric road matrices are supported; diagonal entries must be zero.
- **Scenario Specification (`.ocx`)**: Text format containing fleet parameters (vehicle count, capacity, duration limit, start time), objective weights, node attributes (demand, service duration, time windows), and travel matrices.
- **Route Planner**: Calculates deterministic node arrival, waiting, service initiation ($b_i = \max(arr_i, e_i)$), lateness ($\max(0, b_i - l_i)$), and departure times.

### 4.2. Independent Feasibility Validator (`core/validator.cpp`)

The validator computes route statistics directly from raw scenario matrices, ensuring:
- Rule V01–V03: Node bounds, non-empty routes, and depot start/end matching.
- Rule V04: Exact customer coverage (every customer $c \in C$ served once or listed in unserved list with documented reason).
- Rule V05: Vehicle capacity bounds ($\sum q_i \le Q$).
- Rule V06: Route duration limits ($ret - T_0 \le L$).
- Rule V07–V08: Time window conformance (hard mode enforces $b_i \le l_i$).
- Rule V09: Vehicle count constraint ($|\{v : R_v \neq \emptyset\}| \le K$).
- Rule V10–V12: Numeric consistency of reported travel time, distance, and objective cost index within $10^{-6}$ tolerance.
- Rule V13: Guard against bogus solutions claiming feasibility with zero served requests.

### 4.3. Custom CPU Solver: Chronos (`solvers/cpu/`)

- **Screening**: Flags customers whose demands exceed vehicle capacity or whose time windows cannot be reached from the depot.
- **Initial Construction**: Clarke-Wright savings heuristic with deterministic tie-breaking, followed by greedy cheapest insertion for any unassigned requests.
- **Local Search**: First-improvement intra-route and inter-route neighborhoods:
  - *Relocate* (shift customer to another position or route)
  - *Swap* (exchange two customers)
  - *2-Opt* (reverse sub-tours)
- **Iterated Local Search (ILS)**: Seeded perturbation operator (destroy $k \in [2, 12]$ stops and reinsert) followed by local search; strictly improves or retains incumbent.

### 4.4. Baseline Reference Runner (`solvers/baseline/`)

- Built on Google OR-Tools 9.15 (`pywrapcp`).
- Models CVRPTW with vehicle fixed costs, distance/time dimensions, capacity dimensions, and soft/hard time windows.
- Outputs identical `.ocs` solution files, allowing cross-verification via the C++ CLI validator.

---

## 5. CPU/GPU Responsibility Boundary (Optional CUDA Build)

On machines equipped with the CUDA toolchain (see `docs/CUDA_DESIGN.md`), GPU parallelism is restricted to candidate evaluation:

```
[Host CPU: Chronos / Search Engine]
             │
    Generate candidate route batches (Structure of Arrays)
             │
             ▼
[Host-to-Device Memory Transfer (H2D)]
             │
             ▼
[CUDA Device Kernels: solvers/cuda/]
- Batch candidate route cost evaluation
- Capacity & time-window constraint verification
- Objective reduction across candidate routes
             │
             ▼
[Device-to-Host Memory Transfer (D2H)]
             │
             ▼
[Host CPU: Incumbent Update & Move Selection]
```

- **Current Machine Status**: No CUDA toolchain installed (no MSVC host compiler, no `nvcc`). The production engine operates strictly in CPU mode with full verification and zero fabricated numbers.

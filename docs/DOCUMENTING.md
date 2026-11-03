# DOCUMENTING.md — OptiCore engineering journal

Single source of truth for project status and history. Updated after every
phase and meaningful experiment. Prior results are never overwritten — new
runs are appended.

---

## 1. Objective and scope

**Objective.** Build a CVRPTW optimization engine: deterministic scenario
model, independent feasibility validator, trusted reference baseline
(OR-Tools), a custom C++ heuristic/metaheuristic, a reproducible benchmark
harness, and measured, honestly-reported quality/time comparisons. GPU
acceleration is treated as a hypothesis to test only after the CPU path is
reliable; on the current machine there is no CUDA toolchain (no MSVC, no
nvcc), so the GPU track is documented as an optional build
(`docs/CUDA_DESIGN.md`) rather than claimed.

**Target scenario.** Static, single-depot city delivery fleets over a
synthetic-but-deterministic clustered Euclidean city matrix (times in
minutes, distances in km). No live traffic claims.

**Non-goals (first iteration).** Live traffic data, multi-depot, EV/charging
constraints, guaranteed global optimality on large instances, CUDA where the
workload does not justify it, Kubernetes, a full road-routing engine
replacement.

## 2. Phase 0 environment inventory (recorded 2026-11-03)

| Component | Value |
|---|---|
| OS | Windows 11 Home Single Language, 10.0.26300 x64 |
| CPU | Intel Core i7-13620H (10C/16T), 15.8 GB RAM |
| GPU | NVIDIA GeForce RTX 4060 Laptop 8GB, driver 617.42, CUDA driver (UMD) 13.4, WDDM |
| Disk of C: | 188 GiB free |
| C++ compiler | g++ (MSYS2/UCRT64 MinGW-w64) 16.2.0 |
| Build system | CMake 4.4.4 (winget user-scope) + Ninja 1.13.2 |
| Python | 3.13.15; OR-Tools 9.15.6755 via pip (`ortools.constraint_solver.pywrapcp`) |
| CUDA toolkit | **not installed**; no MSVC (cl.exe) present; MinGW gcc is NOT a valid nvcc host compiler on Windows |
| Git | 2.55.0.windows.3 |

**CUDA feasibility check.** `nvcc` is absent and Visual Studio is absent.
Installing VS 2022 Build Tools + CUDA Toolkit ≤ 13.4 (driver matches 13.4)
would enable the GPU experiment; the user chose CPU-first, CUDA documented
as optional. Consequently the Phase-0 CPU/GPU break-even experiment could
not be run; the equivalent CPU workload probe was run instead (batch route
cost evaluation lives inside `chronos` local search; timings for it are in
`EXPERIMENTS_AND_BENCHMARKS.md`), and the GPU kernel contract + break-even
method are specified in `CUDA_DESIGN.md` without fabricated numbers.

## 3. Architecture summary

- `core/` — domain types + units (types.hpp), scenario/solution file IO
  (frozen v1 text formats), `plan_route` forward scheduling (single truth),
  objective breakdown, independent validator.
- `simulation/` — `city_model` deterministic scenario generator (mt19937_64).
- `solvers/cpu/chronos` — screening → Clarke-Wright savings → cheapest
  insertion → local search (relocate/swap/2-opt) → seeded ILS.
- `solvers/baseline/or_tools_baseline.py` — OR-Tools Routing reference
  producing the same `.ocs` files, parsed/validated by the same core tools.
- `solvers/cuda/` — reserved; see CUDA_DESIGN.md.
- `tools/` — the `opticore` CLI (gen/solve/validate/list-solvers/info).
- `benchmarks/` — committed instances + `run_bench.py` harness.
- `tests/` — bespoke C++ harness + cross-language Python unittests.

CPU/GPU boundary: none at runtime because no GPU build exists; the boundary
design (candidate-batch SoA evaluation) is specified in CUDA_DESIGN.md.

## 4. Technology choices

| Choice | Rationale | Alternatives rejected |
|---|---|---|
| MSYS2 gcc 16.2 + CMake + Ninja | present, working, zero-admin | MSVC (absent), clang (absent) |
| Bespoke frozen text formats (.ocx/.ocs) | deterministic, diffable, parser in both C++ and Python | JSON (needs a hand-rolled parser in C++), protobuf (dep weight) |
| OR-Tools 9.15 as Python subprocess baseline | pip-installed, independent process, C++ validator arbitrates | linking OR-Tools C++ (needs MSVC toolchain here) |
| mt19937_64 seeded RNG (std) | standardized stream across compilers | platform-random APIs |

## 5. Mathematical formulation summary

Detailed in `PROBLEM_FORMULATION.md`. CVRPTW, single depot node 0,
homogeneous fleet K, hard capacity, hard-or-soft time windows, optional
route-duration cap, deterministic objective with five configurable weights
(travel time, distance, vehicle fixed, lateness, unserved penalty).

## 6. Milestone status

| Phase | Milestone | Status |
|---|---|---|
| 0 | environment inventory + toolchain smoke test | **Verified** |
| 0 | CUDA smoke test + CPU/GPU break-even | **Not started** (blocked: no toolchain; documented optional build) |
| 1 | formulation, contracts, architecture docs, ADRs | **Verified** |
| 2 | scenario model + independent validator + tests | **Verified** |
| 3 | OR-Tools baseline producing validator-accepted solutions | **Verified** |
| 4 | custom CPU solver (chronos), deterministic, tested incl. exact tiny optimum | **Verified** |
| 5 | benchmark harness, committed instances, CSV+md results | **Verified** |
| 6 | CUDA experiment | **Not started** (optional build documented) |
| 7 | OSM-based real city data + map visualization | **Not started** |
| 8 | dynamic extensions (EV, multi-depot, disruptions) | **Not started** |
| 9 | hardening, CI, clean-clone build check | **Verified** |

Feature checklist (implemented only):
- [x] Deterministic scenario format + writer/reader in C++ and Python
- [x] Scenario sanity validation (units, ranges, matrix shape)
- [x] Route planner (forward scheduling incl. waiting) shared by everything
- [x] Objective breakdown with 5 weights
- [x] Independent validator (13 rule codes)
- [x] chronos CPU solver, deterministic, seeded, budget-aware
- [x] OR-Tools baseline runner with mirrored planner
- [x] CLI gen/solve/validate/info/list-solvers
- [x] Benchmark harness writing CSV + md, gap computation
- [x] 18 tests: 9 core + 6 solver + 3 cross-language
- [x] Complete documentation suite (15 documents + 6 ADRs)
- [ ] CUDA kernels + CPU/GPU comparison (optional build; no toolchain here)
- [ ] OSM extract ingestion + map visualization

## 7. ADRs

- ADR-001 CPU-first machine; CUDA documented as optional build
- ADR-002 zero third-party C++ dependencies; bespoke deterministic formats
- ADR-003 OR-Tools as external (Python) reference baseline
- ADR-004 objective definition (weights, hard vs soft TW, unserved penalty)
- ADR-005 chronos algorithm selection; ALNS deferred
- ADR-006 single-truth `plan_route` shared by solver, objective, validator

## 8. Bugs found and fixed

| Date | Bug | Root cause | Fix |
|---|---|---|---|
| 2026-11-03 | scenario parser rejected valid `BEGIN_MATRIX_TIME_MIN` | header section state machine lacked a branch for matrix section begins | added explicit branches requiring nodes closed first |
| 2026-11-03 | generated scenarios had structurally short fleets (guaranteed unserved) | auto-capacity used 0.9x mean load per vehicle | changed to 1.2x with max-single-demand floor |
| 2026-11-03 | ILS monotonicity test failed | test compared 250 phases against 120 (inverted) | fixed test semantics |
| 2026-11-03 | tiny-optimum brute force returned cost 0 | unserved count passed as 0 even when assignment dropped customers | count unserved and pass to evaluate |

## 9. Exact commands and outcomes (2026-11-03)

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   → OK (gcc 16.2.0)
cmake --build build                                        → OK, zero warnings (-Wall -Wextra -Wpedantic)
build/oc_tests_core.exe                                    → 9 run, 0 failed
build/oc_tests_solver.exe                                  → 6 run, 0 failed
python -m unittest discover -s tests/python                → Ran 3 tests, OK
build/opticore.exe solve tiny-demo → validate              → OK (chronos cost 412.42, served 8/8)
python solvers/baseline/or_tools_baseline.py → C++ validate → OK (cost 412.42, served 8/8)
python benchmarks/run_bench.py                             → run-20261103-004710 (see EXPERIMENTS doc)
probe-120 (120 customers, 18 vehicles):                    → feasible, all 120 served, solve 9.45s,
                                                             validate 0.05s, validator: OK
python benchmarks/run_bench.py (repeat)                    → run-20261103-091855, verified identical costs
```

## 10. Known limitations and caveats

- No CUDA: nothing in this repo claims GPU speedups; GPU code is a
  documented optional build only.
- OR-Tools internal costs use milli-unit scaling (integers) while the
  validator recomputes in doubles; decisions could theoretically diverge on
  near-tied costs — acceptable, documented.
- chronos time-budget stopping is wall-clock (nondeterministic across
  machine speeds); deterministic stopping exists via `--max-iterations`.
- Benchmark wall times include process startup (~40 ms); single machine,
  single run per line — repeated runs should be recorded as new entries.
- Travel matrices are static and synthetic; no live traffic claims.
- No frontend/visualization yet; no backend/API yet (per scope).
- Validator rule: solution claiming `feasible`/`timeout_incumbent` with zero
  served customers is invalid (`nothing_served`).

## 11. Next actions (smallest useful first)

1. (optional toolchain) install VS Build Tools + CUDA ≤13.4, then follow
   CUDA_DESIGN.md Phase A (candidate-cost batch kernel + CPU reference).
2. OSM extract ingestion into the matrix interface (fx parsed road graph →
   travel-time model), with ODbL attribution recorded in
   DATA_AND_CITY_MODEL.md.
3. sanitizer sweep (`-fsanitize=address,undefined`) on tests.

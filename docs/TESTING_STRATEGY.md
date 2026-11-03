# TESTING_STRATEGY.md — Testing and Quality Assurance Strategy

This document details the multi-tiered testing strategy, assertion harnesses, coverage areas, and validation invariants for OptiCore.

---

## 1. Test Architecture Overview

OptiCore employs an 18-test suite spanning C++ domain units, algorithmic convergence, and cross-language baseline integration:

```
[OptiCore Test Suite (18 tests total)]
  ├── C++ Core Unit Tests (9 tests): tests/test_core.cpp (oc_tests_core.exe)
  ├── C++ Solver Tests (6 tests):    tests/test_solver.cpp (oc_tests_solver.exe)
  └── Python Integration (3 tests):  tests/python/test_baseline.py
```

All C++ test suites use a zero-dependency header assertion harness (`tests/harness/tester.hpp`) supporting `ASSERT_TRUE`, `ASSERT_EQ`, `ASSERT_NEAR`, and structured failure reports.

---

## 2. Test Suites and Invariant Coverage

### 2.1. Core Unit Tests (`oc_tests_core.exe`)
1. **Types & Units**: Verifies representation of node IDs, floating-point coordinates, and objective weight structures.
2. **Scenario Text Parser**: Tests reading and writing `.ocx` format, header parsing, matrix section headers (`BEGIN_MATRIX_TIME_MIN`, `BEGIN_MATRIX_DIST_KM`), and rejection of malformed files.
3. **Route Planner Schedule**: Validates forward scheduling logic, waiting time calculations, service start bounds ($b_i = \max(arr_i, e_i)$), and return time to depot.
4. **Objective Calculation**: Verifies linear combination of travel time, distance, vehicle fixed cost, lateness penalty, and unserved penalties.
5. **Validator - Valid Solution**: Confirms acceptance of compliant multi-route solutions.
6. **Validator - Capacity Exceeded**: Injects excess load and verifies trigger of rule code `V05`.
7. **Validator - Duration Exceeded**: Injects route elapsed time exceeding shift duration and verifies rule code `V06`.
8. **Validator - Time Window Violated**: Injects customer arrival past $l_i$ in hard TW mode and verifies rule code `V08`.
9. **Validator - Guard Against Empty Solutions**: Asserts that solutions claiming `feasible` or `timeout_incumbent` with zero served customers fail with `nothing_served`.

### 2.2. Solver Correctness Tests (`oc_tests_solver.exe`)
1. **Pre-Screening**: Asserts that unservable customers (demand $> Q$ or depot-to-customer time $> L$) are immediately flagged as unserved without corrupting search.
2. **Clarke-Wright Savings Construction**: Verifies initial route generation respects capacity and produces non-overlapping stop assignments.
3. **Local Search Improvement**: Asserts that Relocate, Swap, and 2-Opt strictly improve or maintain candidate cost.
4. **Iterated Local Search (ILS) Monotonicity**: Verifies that 250 phases of ILS produce cost $\le$ 50 phases (incumbent is never degraded).
5. **Seeded Determinism**: Runs the solver twice with identical seed; verifies byte-identical `.ocs` output.
6. **Exact Tiny Optimum Verification**: Compares Chronos against brute-force enumeration on a 5-customer instance, asserting exact convergence to global optimum cost ($164.0$).

### 2.3. Cross-Language Baseline Integration (`test_baseline.py`)
1. **Scenario Compatibility**: Verifies Python parser reads `.ocx` written by C++ CLI.
2. **OR-Tools Execution**: Solves sample scenario using OR-Tools 9.15 and writes valid `.ocs`.
3. **C++ Independent Validation**: Invokes `opticore validate` CLI on the OR-Tools solution to verify cross-system validation agreement.

---

## 3. Execution Commands

```powershell
# Run all C++ unit and solver tests
ctest --test-dir build --output-on-failure

# Run C++ executables directly
./build/oc_tests_core.exe
./build/oc_tests_solver.exe

# Run cross-language Python tests
python -m unittest discover -s tests/python
```

---

## 4. Compiler Warnings and Sanitizer Hygiene

- **Compiler Flags**: Core and tests compile with `-std=c++20 -O2 -Wall -Wextra -Wpedantic` with zero warnings under GCC 16.2.0.
- **Sanitizers**: AddressSanitizer (`-fsanitize=address,undefined`) is recommended for memory bound verification and integer overflow audits during release testing.

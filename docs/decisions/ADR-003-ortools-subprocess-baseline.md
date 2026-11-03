# ADR-003: Google OR-Tools as External Baseline Subprocess

## Status
Accepted

## Context
A primary requirement of OptiCore is establishing a reliable, industry-standard reference baseline to benchmark solution quality and solver runtimes. Google OR-Tools is the industry gold standard for Operations Research and CVRPTW.

However, linking Google OR-Tools directly into C++ on Windows requires prebuilt binary packages that depend on MSVC ABI compatibility, conflicting with the current MSYS2/GCC C++20 toolchain.

## Decision
1. Deploy Google OR-Tools (v9.15) via its official Python package (`pip install ortools`).
2. Implement a dedicated baseline runner script (`solvers/baseline/or_tools_baseline.py`) that reads `.ocx` files, solves them using OR-Tools' Routing library (`PATH_CHEAPEST_ARC` + `GUIDED_LOCAL_SEARCH`), and outputs standard `.ocs` files.
3. Validate the resulting `.ocs` files using the independent C++ validator (`opticore.exe validate`), treating OR-Tools as an external black-box reference.

## Consequences
- **Positive**: Clean separation of concerns; avoids C++ ABI linkage friction; the validator treats the reference baseline identically to the custom C++ solver.
- **Negative**: Benchmark wall times for OR-Tools include Python interpreter startup (~100 ms), though this is negligible compared to the multi-second optimization budgets (5 s–30 s).

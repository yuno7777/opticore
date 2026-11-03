# DEPLOYMENT.md — Build, Run, and Deployment Guide

This document describes how to build, run, test, and deploy OptiCore from a clean clone.

---

## 1. Prerequisites and Toolchains

### Windows (Tested Environment)
- **C++ Compiler**: GCC 16.2.0 (MSYS2 UCRT64) or compatible C++20 compiler.
- **Build System**: CMake $\ge 3.20$ (tested with 4.4.4) + Ninja $\ge 1.13$.
- **Python**: Python $\ge 3.10$ (tested with 3.13.15).
- **Optional Baseline**: `pip install ortools` (tested with 9.15.6755).

### Linux (Ubuntu 22.04 / 24.04)
```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build python3 python3-pip
python3 -m pip install ortools
```

---

## 2. Clean-Clone Build Steps

```powershell
# 1. Clone repository
git clone https://github.com/example/opticore.git
cd opticore

# 2. Configure with CMake and Ninja (Release mode)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# 3. Build all targets (core library, CLI, and test suites)
cmake --build build

# 4. Run test suites
ctest --test-dir build --output-on-failure
python -m unittest discover -s tests/python
```

---

## 3. Running Scenarios

```powershell
# Generate a synthetic 20-customer scenario
./build/opticore.exe gen --out scenario-20.ocx --seed 42 --customers 20 --name demo-20

# Solve with Chronos CPU solver
./build/opticore.exe solve --scenario scenario-20.ocx --solution solution-20.ocs --seed 42

# Independently validate the solution
./build/opticore.exe validate --scenario scenario-20.ocx --solution solution-20.ocs

# Solve with OR-Tools reference baseline
python solvers/baseline/or_tools_baseline.py --scenario scenario-20.ocx --solution solution-ortools-20.ocs --time-limit-s 10

# Validate OR-Tools output with C++ validator
./build/opticore.exe validate --scenario scenario-20.ocx --solution solution-ortools-20.ocs
```

---

## 4. Running the Benchmark Suite

```powershell
python benchmarks/run_bench.py
```
This script executes all committed benchmark instances (`tiny-8`, `small-20`, `medium-50`) across both solvers, validates every solution using `opticore.exe validate`, and outputs a timestamped CSV and Markdown summary to `benchmarks/results/`.

---

## 5. Optional Docker Containerization (Phase 7/8 Service)

A multi-stage Docker build can package the engine:

```dockerfile
FROM gcc:14 AS builder
RUN apt-get update && apt-get install -y cmake ninja-build
WORKDIR /app
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build

FROM python:3.13-slim
WORKDIR /app
COPY --from=builder /app/build/opticore /usr/local/bin/opticore
COPY --from=builder /app/solvers/baseline /app/solvers/baseline
RUN pip install --no-cache-dir ortools
ENTRYPOINT ["opticore"]
```

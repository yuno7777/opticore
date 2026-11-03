# API_SPECIFICATION.md — CLI Interface and Planned Service API

This document specifies the current production command-line interface (CLI) for OptiCore and defines the planned REST API contracts for future service deployment.

---

## 1. Scope and Strategy

Per the OptiCore Master Specification (Section 6), no database, REST API, or frontend is introduced until the CLI solver and benchmark harness are fully proven. The primary production API of OptiCore is the `opticore` CLI.

---

## 2. Production CLI Interface (`opticore.exe`)

The unified CLI binary provides scenario generation, optimization, validation, and metadata queries:

### 2.1. `opticore info`
Prints build information, compiler version, target architecture, and feature flags.

```powershell
build/opticore.exe info
```
**Output**:
```text
OptiCore Urban Logistics Optimization Engine v1.0.0 (C++20)
Compiler: GNU 16.2.0
Build Type: Release
CUDA Support: Disabled (CPU-only build)
```

### 2.2. `opticore gen`
Synthesizes a reproducible, clustered city scenario and writes an `.ocx` file.

```powershell
build/opticore.exe gen --out <path> --seed <uint64> --customers <int> [--clusters <int>] [--vehicles <int>] [--capacity <int>] [--name <str>]
```
**Options**:
- `--out`: Output `.ocx` file path (required).
- `--seed`: Pseudorandom seed (default: 42).
- `--customers`: Number of delivery customers (default: 20).
- `--clusters`: Number of geographic customer clusters (default: 3).
- `--vehicles`: Homogeneous vehicle fleet size (default: auto).
- `--capacity`: Vehicle load capacity (default: auto).
- `--name`: Scenario identifier (default: `synthetic-city`).

### 2.3. `opticore solve`
Solves a CVRPTW scenario using the specified algorithm and writes an `.ocs` file.

```powershell
build/opticore.exe solve --scenario <input.ocx> --solution <output.ocs> [--solver <name>] [--seed <uint64>] [--time-budget-s <double>] [--max-iterations <int>]
```
**Options**:
- `--scenario`: Path to input `.ocx` scenario (required).
- `--solution`: Path to output `.ocs` solution (required).
- `--solver`: Solver engine (`chronos-cpu` [default]).
- `--seed`: Deterministic search seed (default: 42).
- `--time-budget-s`: Wall-clock execution limit in seconds (default: 0 = disabled).
- `--max-iterations`: Maximum search iterations / ILS phases (default: 250).

### 2.4. `opticore validate`
Runs the independent feasibility validator against an `.ocs` solution using raw `.ocx` data.

```powershell
build/opticore.exe validate --scenario <input.ocx> --solution <input.ocs> [--verbose]
```
**Exit Code**:
- `0`: Solution is completely valid.
- `1`: Validation failed (rule violation details printed to stderr).

### 2.5. `opticore list-solvers`
Lists all available solver backends compiled into the binary.

```powershell
build/opticore.exe list-solvers
```

---

## 3. Planned REST API Specification (Phase 7/8 Future Extension)

When OptiCore is wrapped in a microservice (e.g., via FastAPI or a C++ Drogon/Crow server), it will expose the following REST endpoints:

### 3.1. `POST /api/v1/scenarios`
Upload or generate a new scenario.

**Request Body**:
```json
{
  "name": "downtown-delivery",
  "seed": 404,
  "fleet": { "vehicle_count": 5, "capacity": 100, "duration_limit_min": 480.0 },
  "objective_weights": { "time": 1.0, "dist": 0.0, "fixed": 100.0, "late": 500.0, "unserved": 10000.0 },
  "nodes": [
    { "id": 0, "demand": 0, "service_min": 0.0, "tw_early": 0.0, "tw_late": 480.0 },
    { "id": 1, "demand": 15, "service_min": 10.0, "tw_early": 30.0, "tw_late": 120.0 }
  ],
  "matrix_time_min": [[0.0, 12.5], [12.5, 0.0]],
  "matrix_dist_km": [[0.0, 6.2], [6.2, 0.0]]
}
```

### 3.2. `POST /api/v1/jobs`
Enqueue an asynchronous optimization task.

**Request Body**:
```json
{
  "scenario_id": "scen_98234ab",
  "solver": "chronos-cpu",
  "seed": 42,
  "time_budget_s": 10.0
}
```

**Response (202 Accepted)**:
```json
{
  "job_id": "job_01h89x",
  "status": "queued",
  "created_at": "2026-11-03T14:40:00Z"
}
```

### 3.3. `GET /api/v1/jobs/{job_id}`
Poll job progress or retrieve completed routes, cost breakdown, and independent validation status.

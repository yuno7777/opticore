# SIMULATION_AND_SCENARIOS.md — Simulation Generator and Instance Catalogue

This document describes the simulation engine, parameters, seed guarantees, and standardized benchmark instance catalogue for OptiCore.

---

## 1. Simulation Engine (`simulation/city_model.cpp`)

The generator synthesizes urban logistics instances with clustered demand patterns, bounded vehicle capacities, and realistic service time windows.

### CitySpec Parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `name` | string | `"synthetic-city"` | Identifier written into `.ocx` header |
| `seed` | uint64 | `42` | Random seed driving `std::mt19937_64` |
| `customer_count` | int | `20` | Total delivery customers (excluding depot 0) |
| `cluster_count` | int | `3` | Number of geographic distribution centers |
| `map_radius_km` | double | `15.0` | Maximum radius from central depot (km) |
| `vehicle_count` | int | `5` | Homogeneous fleet size available |
| `vehicle_capacity` | int | `100` | Load units each vehicle can carry |
| `max_route_duration_min` | double | `480.0` | Maximum route duration (8-hour shift) |
| `avg_speed_kmh` | double | `30.0` | Average urban transit speed |
| `min_demand` | int | `5` | Minimum customer order size |
| `max_demand` | int | `25` | Maximum customer order size |
| `time_window_span_min` | double | `120.0` | Duration of customer time window ($l_i - e_i$) |

---

## 2. Determinism and Portability Guarantee

1. **Standardized Pseudo-Random Stream**: All random draws (cluster assignments, coordinates, demands, service durations, window offsets) utilize `std::mt19937_64` initialized with the user-supplied 64-bit seed.
2. **Standard Distributions**: Only `std::uniform_real_distribution`, `std::uniform_int_distribution`, and `std::normal_distribution` are used.
3. **Byte-Stable Output**: Floats are formatted with consistent decimal precision (`std::fixed`, 4–6 decimal places), producing bit-stable `.ocx` files across MSYS2/GCC, Clang, and MSVC.

---

## 3. Standardized Instance Catalogue

The `benchmarks/instances/` directory contains committed, version-controlled benchmark instances:

### 3.1. `tiny-8` (`benchmarks/instances/tiny-8.ocx`)
- **Customers**: 8
- **Clusters**: 2
- **Fleet**: 3 vehicles (Capacity 60)
- **Time Windows**: Hard
- **Purpose**: Fast smoke testing, exact brute-force validation ($N!$ checks), and quick CI execution.

### 3.2. `small-20` (`benchmarks/instances/small-20.ocx`)
- **Customers**: 20
- **Clusters**: 3
- **Fleet**: 5 vehicles (Capacity 100)
- **Time Windows**: Hard
- **Purpose**: Standard testbed for constructive heuristics and local search neighborhoods.

### 3.3. `medium-50` (`benchmarks/instances/medium-50.ocx`)
- **Customers**: 50
- **Clusters**: 4
- **Fleet**: 10 vehicles (Capacity 140)
- **Time Windows**: Hard
- **Purpose**: Metaheuristic scaling evaluation, comparing Chronos Iterated Local Search against OR-Tools Guided Local Search.

### 3.4. `probe-120` (Stress Instance)
- **Customers**: 120
- **Clusters**: 6
- **Fleet**: 18 vehicles (Capacity 180)
- **Time Windows**: Hard
- **Purpose**: Stress test measuring memory stability and large-scale solve time ($9.45\text{ s}$ solve, $0.05\text{ s}$ validate).

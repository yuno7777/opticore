# DATA_AND_CITY_MODEL.md — Data Model, Formats, and City Representation

This document details the city road network model, synthetic scenario generator, scenario and solution file specifications, and data licensing policies for OptiCore.

---

## 1. City and Road Network Representation

OptiCore represents urban geography via discrete nodes and dense cost matrices rather than requiring an active GIS routing server in the core loop:

- **Node 0 (Depot)**: The central distribution hub where all active vehicles must start and return.
- **Nodes $1 \dots N-1$ (Delivery Customers)**: Distinct locations requiring delivery.
- **Travel Matrices**:
  - **Travel Time Matrix ($t_{ij}$)**: Stored in double precision, measured in **minutes**.
  - **Travel Distance Matrix ($d_{ij}$)**: Stored in double precision, measured in **kilometers**.
  - **Asymmetry**: Real urban road networks feature one-way streets, turn restrictions, and directional congestion. OptiCore explicitly permits $t_{ij} \neq t_{ji}$ and $d_{ij} \neq d_{ji}$.
  - **Diagonal**: $t_{ii} = 0.0$ and $d_{ii} = 0.0$ for all $i \in N$.
  - **Triangle Inequality**: Not strictly assumed, reflecting urban ring roads or highway detours.

### Real-World vs. Synthetic Data Status

- **Current Implementation**: Deterministic clustered Euclidean generator (`simulation/city_model.cpp`). Customer locations are sampled around spatial cluster centers to simulate realistic suburban and commercial customer density.
- **Live Traffic Status**: **Explicit non-goal**. OptiCore makes no claim of integrating live traffic telemetry. All travel times represent static or simulated operational baselines.
- **OpenStreetMap Licensing Policy**: When OSM extracts (PBF/OSRM/Valhalla) are integrated in Phase 7, OpenStreetMap contributors must be prominently attributed per the Open Database License (ODbL 1.0).

---

## 2. Synthetic Scenario Generator (`simulation/`)

Scenarios are deterministically generated using standard 64-bit Mersenne Twister RNG (`std::mt19937_64`):

```cpp
struct CitySpec {
    std::string name;
    std::uint64_t seed = 42;
    int customer_count = 20;
    int cluster_count = 3;
    double map_radius_km = 15.0;
    int vehicle_count = 5;
    int vehicle_capacity = 100;
    double max_route_duration_min = 480.0; // 8 hours
    double avg_speed_kmh = 30.0;
    int min_demand = 5;
    int max_demand = 25;
    double time_window_span_min = 120.0;
};
```

1. **Spatial Clustering**: Customer $(x, y)$ coordinates are generated around $K$ normal distribution centers within a bounding radius.
2. **Matrix Derivation**: $d_{ij} = \sqrt{(x_i - x_j)^2 + (y_i - y_j)^2}$, and $t_{ij} = \frac{d_{ij}}{\text{speed}} \times 60$.
3. **Time Windows**: Window start $e_i$ is distributed across an 8-hour operational day with a configurable duration span ($l_i = e_i + \text{span}$), ensuring feasible arrival times from the depot.
4. **Fleet Sizing**: Vehicle capacity is dimensioned with an auto-capacity margin ($\approx 1.2\times$ average load) to avoid structural infeasibility while maintaining tight capacity constraints.

---

## 3. Scenario File Specification (`.ocx`)

`.ocx` is an explicit, line-oriented, UTF-8 text format designed to be human-readable, easily parsed without external libraries, and diff-friendly:

```text
# opticore scenario v1
NAME tiny-8
SEED 7
VEHICLES 3
CAPACITY 60
DURATION_LIMIT 480.000000
START_TIME 0.000000
TW_MODE hard
WEIGHTS time 1.000000 dist 0.000000 fixed 100.000000 late 500.000000 unserved 10000.000000
NODES 9
# id x_km y_km demand service_min tw_early_min tw_late_min
0 0.0000 0.0000 0 0.0000 0.0000 480.0000
1 2.3400 4.1200 12 10.0000 30.0000 150.0000
...
BEGIN_MATRIX_TIME_MIN
0.0000 9.4800 12.1100 ...
9.4800 0.0000 5.2200 ...
...
END_MATRIX_TIME_MIN
BEGIN_MATRIX_DIST_KM
0.0000 4.7400 6.0550 ...
4.7400 0.0000 2.6100 ...
...
END_MATRIX_DIST_KM
```

---

## 4. Solution File Specification (`.ocs`)

`.ocs` documents solver output, route stop sequences, performance statistics, and unserved customer manifests:

```text
# opticore solution v1
scenario tiny-8
solver chronos-cpu
seed 7
status feasible
wall_time_s 0.046120
cost_total 412.420000
cost_time 212.420000
cost_dist 0.000000
cost_fixed 200.000000
cost_late 0.000000
cost_unserved 0.000000
BEGIN_ROUTES
vehicle 0 0 1 4 2 0
vehicle 1 0 3 5 0
END_ROUTES
BEGIN_UNSERVED
# id reason
END_UNSERVED
```

- **Stop Order**: A route sequence starts and ends at node 0 (the depot). Intermediate customer IDs are space-separated.
- **Unserved Section**: Any request that could not be feasibly assigned must be explicitly listed with a reason (e.g., `demand_exceeds_capacity`, `no_feasible_insertion_found`).

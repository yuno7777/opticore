# DATABASE_SCHEMA.md — Persistence Architecture and Relational Schema

This document describes the current file-based persistence model and the target relational schema (PostgreSQL + PostGIS) for future service-level persistence.

---

## 1. Current Persistence Strategy (File-Based)

In accordance with Phase 0–5 specifications, OptiCore avoids heavy relational database dependencies in the core solver loop. Scenarios and solutions are persisted as structured flat files:

- **Scenario Artifacts (`.ocx`)**: Immutable input records containing node tables, time windows, and travel matrices.
- **Solution Artifacts (`.ocs`)**: Timestamped solver runs containing ordered vehicle routes, component costs, and unserved request lists.
- **Benchmark Artifacts (`.csv`, `.md`)**: Aggregated metrics indexed by instance name, solver, wall-clock duration, and solution cost.

This file-based approach guarantees zero-overhead reproducible builds, fast CI runs, and transparent diff tracking in Git.

---

## 2. Target Relational Schema (PostgreSQL 16 + PostGIS)

When OptiCore scales to multi-user web/API deployment in Phase 7/8, the following schema will persist scenarios, city graphs, road networks, and optimization runs:

```sql
-- Enable PostGIS extension for geospatial routing
CREATE EXTENSION IF NOT EXISTS postgis;

-- 1. Road Networks / Cities
CREATE TABLE cities (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(128) NOT NULL,
    bounding_box GEOMETRY(Polygon, 4326),
    source_attribution TEXT NOT NULL, -- e.g., 'OpenStreetMap contributors (ODbL)'
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- 2. Scenarios
CREATE TABLE scenarios (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    city_id UUID REFERENCES cities(id) ON DELETE CASCADE,
    name VARCHAR(128) NOT NULL,
    seed BIGINT NOT NULL,
    vehicle_count INT NOT NULL CHECK (vehicle_count > 0),
    vehicle_capacity INT NOT NULL CHECK (vehicle_capacity > 0),
    duration_limit_min DOUBLE PRECISION,
    tw_mode VARCHAR(16) NOT NULL DEFAULT 'hard', -- 'hard' or 'soft'
    w_time DOUBLE PRECISION NOT NULL DEFAULT 1.0,
    w_dist DOUBLE PRECISION NOT NULL DEFAULT 0.0,
    w_fixed DOUBLE PRECISION NOT NULL DEFAULT 100.0,
    w_late DOUBLE PRECISION NOT NULL DEFAULT 500.0,
    w_unserved DOUBLE PRECISION NOT NULL DEFAULT 10000.0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- 3. Scenario Nodes (Depot and Delivery Customers)
CREATE TABLE scenario_nodes (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    scenario_id UUID NOT NULL REFERENCES scenarios(id) ON DELETE CASCADE,
    node_index INT NOT NULL, -- 0 = depot
    geom GEOMETRY(Point, 4326) NOT NULL,
    demand INT NOT NULL CHECK (demand >= 0),
    service_duration_min DOUBLE PRECISION NOT NULL CHECK (service_duration_min >= 0.0),
    tw_early_min DOUBLE PRECISION NOT NULL,
    tw_late_min DOUBLE PRECISION NOT NULL,
    CONSTRAINT uq_scenario_node UNIQUE (scenario_id, node_index)
);

-- 4. Optimization Jobs / Runs
CREATE TABLE optimization_runs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    scenario_id UUID NOT NULL REFERENCES scenarios(id) ON DELETE CASCADE,
    solver_name VARCHAR(64) NOT NULL, -- 'chronos-cpu', 'ortools-baseline', etc.
    seed BIGINT NOT NULL,
    status VARCHAR(32) NOT NULL, -- 'running', 'feasible', 'timeout_incumbent', 'infeasible', 'failed'
    wall_time_seconds DOUBLE PRECISION,
    cost_total DOUBLE PRECISION,
    cost_time DOUBLE PRECISION,
    cost_dist DOUBLE PRECISION,
    cost_fixed DOUBLE PRECISION,
    cost_late DOUBLE PRECISION,
    cost_unserved DOUBLE PRECISION,
    is_validated BOOLEAN NOT NULL DEFAULT FALSE,
    validation_code VARCHAR(32),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMPTZ
);

-- 5. Solution Routes
CREATE TABLE solution_routes (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    run_id UUID NOT NULL REFERENCES optimization_runs(id) ON DELETE CASCADE,
    vehicle_index INT NOT NULL,
    stop_sequence INT[] NOT NULL, -- e.g., ARRAY[0, 5, 2, 8, 0]
    total_travel_time_min DOUBLE PRECISION NOT NULL,
    total_distance_km DOUBLE PRECISION NOT NULL,
    total_demand INT NOT NULL,
    elapsed_duration_min DOUBLE PRECISION NOT NULL
);

-- 6. Unserved Requests
CREATE TABLE unserved_requests (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    run_id UUID NOT NULL REFERENCES optimization_runs(id) ON DELETE CASCADE,
    node_index INT NOT NULL,
    reason VARCHAR(128) NOT NULL
);

-- Indexes for performance
CREATE INDEX idx_scenario_nodes_geom ON scenario_nodes USING GIST (geom);
CREATE INDEX idx_runs_scenario ON optimization_runs (scenario_id);
```

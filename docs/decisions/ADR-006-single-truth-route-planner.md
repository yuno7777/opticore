# ADR-006: Single-Truth Route Schedule Calculation

## Status
Accepted

## Context
In CVRPTW engines, subtle discrepancies frequently arise between how solvers evaluate incremental route costs and how validators compute final feasibility. For instance, differing handling of waiting time, service duration, depot departure, or floating-point precision can cause a solver to believe a route is feasible when the validator rejects it as violating a time window.

## Decision
1. Establish a single, canonical schedule computation function:
   `core::plan_route(...)` in `core/include/opticore/route_planner.hpp`.
2. Mandate that all components—the Chronos constructive heuristic, the local search delta evaluation, the objective function calculator, and the independent validator—call `plan_route` for all forward simulation of arrival times, service start, waiting time, lateness, and duration.
3. Prohibit custom inline forward schedule loops inside solvers.

## Consequences
- **Positive**: Eliminates solver-validator discrepancies; simplifies debugging; ensures that any improvement in schedule fidelity immediately benefits both search and validation.
- **Negative**: Recomputing the full route schedule during local search incurs a minor function call overhead compared to delta-maintenance, but modern compiler inlining and short route lengths ($10\text{–}30$ stops) make this negligible.

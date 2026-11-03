# PROBLEM_FORMULATION.md

First supported formulation: **CVRPTW** — Capacitated Vehicle Routing
Problem with Time Windows, one depot, static travel matrices.

## Sets and parameters

| Symbol | Meaning | Unit | Notes |
|---|---|---|---|
| $N = \\{0..n-1\\}$ | node set | — | node 0 is the depot |
| $C \\subseteq N$ | customers ($demand > 0$) | — | |
| $q_i$ | demand of node $i$ | load units | integer |
| $Q$ | vehicle capacity | load units | integer, hard |
| $K$ | vehicle count | — | homogeneous, integer |
| $t_{ij}$ | travel time $i \\to j$ | minutes | double, may be asymmetric |
| $d_{ij}$ | travel distance $i \\to j$ | kilometers | double, may be asymmetric |
| $s_i$ | service duration at $i$ | minutes | depot: 0 |
| $e_i, l_i$ | time window bounds | minutes | service start must lie within |
| $T_0$ | vehicle start time | minutes | default 0 |
| $L$ | max route duration | minutes | optional ($L = -1$ disables) |
| $\\pi$ | objective weights | cost index / (unit·minute/km/vehicle) | see objective |

Decision variables: ordered stop sequences $R_v$ per vehicle $v \\in K$ and
an unserved set $U \\subseteq C$; arrival/service/departure times are
**derived** by the forward schedule below, not decided.

Notation: a route's stop sequence excludes depot endpoints implicitly;
schedule for stops $(c_1, ..., c_m)$ of one vehicle:

- arrival at first stop: $arr_1 = T_0 + t_{0,c_1}$
- service start: $b_c = \\max(arr_c, e_c)$  (waiting allowed, counts to elapsed)
- lateness: $late_c = \\max(0, b_c - l_c)$
- departure: $b_c + s_c$; next arrival $arr_{k+1} = b_k + s_k + t_{c_k,c_{k+1}}$
- return to depot: $ret = b_m + s_m + t_{c_m,0}$
- route elapsed: $elapsed = ret - T_0$ (includes waiting and service)

## Objective (cost index, dimensionless)

Minimize

```
cost = w_time  * Σ_v travel_time(R_v)   [minutes]
     + w_dist  * Σ_v travel_dist(R_v)   [km]
     + w_fixed * |{v : R_v ≠ ∅}|        [vehicles]
     + w_late  * Σ_v Σ_c late_c         [minutes]
     + w_uns   * |U|                    [customers]
```

Defaults: `w_time=1.0, w_dist=0.0, w_fixed=100.0, w_late=500.0,
w_uns=10000.0` (settable per scenario in the `.ocx` header).

## Constraints

| # | Constraint | Hard/soft |
|---|---|---|
| C1 | each customer is served by exactly one vehicle or listed in U with a reason | hard (structural) |
| C2 | Σ demand on $R_v$ ≤ $Q$ | hard |
| C3b | $b_c \\le l_c$ for all stops (no lateness) | hard if `time_window_mode=hard`; soft (penalized) if `soft` |
| C3a | waiting until $e_c$ is allowed | always |
| C4 | $elapsed_v \\le L$ when $L \\ge 0$ | hard |
| C5 | every used vehicle starts and ends at node 0 | structural |
| C6 | at most $K$ used vehicles | hard |

## Feasibility and status semantics

- A **solution** is an ordered route list + unserved list with per-request
  reasons. Partial coverage is representable and validated: $C1$ holds via
  the unserved list; solutions are not rejected merely for being unhelpful.
- Solver status: `feasible` (finished by convergence/phase budget, ≥1
  served), `infeasible` (nothing served; customers exist), `timeout_incumbent`
  (wall-clock budget expired, ≥1 served), `failed` (internal error).
- The independent validator recomputes $b_c$, $late_c$, loads, elapsed,
  distances and times **from the raw matrix** and compares with claimed
  components (tolerance: `|a-b| ≤ 1e-6·(1+|b|)`); a solver report is never
  accepted on its own.
- `nothing_served`: a solution declaring `feasible`/`timeout_incumbent` with
  zero served customers is invalid.

## Determinism

- Generator and solvers are seeded (`mt19937_64`, standardized stream).
- Enumeration and tie-breaking are deterministic (documented in
  ALGORITHMS.md). With `time_budget_s = 0` runs are bit-reproducible.
- Wall-clock budgets are the only nondeterministic stopping condition;
  comparisons fix the budget equal across solvers.

## Unit conventions (binding)

- distances: kilometers; time: minutes from scenario $t=0$;
  demands/capacity: integer load units; cost: dimensionless "cost index".
- Matrix diagonal must be exactly 0; matrices may be asymmetric; triangle
  inequality is NOT assumed.

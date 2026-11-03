# ALGORITHMS.md — Optimization Algorithms and Baseline Reference

This document describes the algorithmic design, search neighborhoods, time complexity, and reference baseline implementation for OptiCore.

---

## 1. Algorithm Selection Ladder

To establish trustworthy performance bounds, OptiCore employs a strict progression of solvers:

1. **Exact Brute-Force Enumerator (Tests)**: Validates global optimality for tiny instances ($N \le 8$), producing the verified minimum cost $164.0$ used in regression tests (`tests/test_solver.cpp`).
2. **Google OR-Tools 9.15 (External Reference Baseline)**: Industry-standard Constraint Programming / Local Search engine (`solvers/baseline/or_tools_baseline.py`).
3. **Chronos Custom C++ Heuristic / Metaheuristic**: Standalone C++20 solver combining constructive savings, multi-neighborhood local search, and iterated local search (ILS).

---

## 2. Chronos Solver Architecture (`solvers/cpu/chronos.cpp`)

Chronos executes a multi-stage optimization pipeline:

```
[Raw Scenario]
      │
      ▼
[Phase 1: Pre-Screening]
  - Eliminate customers where demand > capacity
  - Eliminate customers where depot -> customer -> depot is infeasible
      │
      ▼
[Phase 2: Constructive Savings (Clarke-Wright)]
  - Compute savings s_ij = t_0i + t_0j - t_ij
  - Sort deterministically: descending s_ij, tie-break by i, then j
  - Greedily merge routes respecting capacity, duration, and time windows
      │
      ▼
[Phase 3: Cheapest Feasible Insertion]
  - For unassigned customers, evaluate all routes and insertion slots
  - Insert customer with minimal incremental objective cost
      │
      ▼
[Phase 4: Variable Neighborhood Descent (Local Search)]
  - Relocate (intra-route and inter-route single customer shifts)
  - Swap (exchange positions of two customers)
  - 2-Opt (intra-route edge swap and sub-tour reversal)
  - First-improvement acceptance until local minimum
      │
      ▼
[Phase 5: Iterated Local Search (ILS)]
  - Perturbation: seeded random removal of k in [2, 12] stops
  - Re-insertion: greedy best insertion of removed stops
  - Local Search: polish perturbed solution
  - Acceptance Criterion: strict improvement only (incumbent is never degraded)
```

### Determinism and Stopping Conditions

- **Determinism**: Given the same random seed and execution parameters, Chronos is 100% bit-reproducible.
- **Time Budget**: When `--time-budget-s` is set, the ILS loop periodically checks elapsed wall time. Because wall time can vary with CPU thermal throttling and OS scheduler jitter, strictly reproducible comparisons should fix `--max-iterations` instead.

---

## 3. Algorithmic Complexity

| Component | Worst-Case Time Complexity | Practical Behavior |
|---|---|---|
| Pre-screening | $O(N)$ | $< 0.1\text{ ms}$ |
| Clarke-Wright Savings | $O(N^2 \log N)$ | Matrix scan + quicksort ($N \le 500$: instant) |
| Cheapest Insertion | $O(U \cdot K \cdot \bar{m}^2)$ | Fast greedy recovery for orphaned stops |
| Relocate Operator | $O(N \cdot K \cdot \bar{m})$ | First-improvement aborts early on profitable moves |
| Swap Operator | $O(N^2)$ | Evaluates candidate customer exchanges |
| 2-Opt Operator | $O(K \cdot \bar{m}^2)$ | Evaluates sub-tour reversals within each route |
| Iterated Local Search | $O(P \cdot (k \cdot K \bar{m} + T_{\text{LS}}))$ | Runs for $P$ phases (default 250) or until time expires |

*Notation: $N$ = customer count, $K$ = vehicle fleet, $\bar{m} \approx N/K$ = average stops per route, $U$ = unserved count, $k$ = perturbation destroy size ($2 \dots 12$), $P$ = ILS phases.*

---

## 4. Google OR-Tools Reference Baseline (`solvers/baseline/`)

The baseline uses `ortools.constraint_solver.pywrapcp`:

- **Cost Scaling**: Because OR-Tools operates in 64-bit integer costs, float travel times and distances are scaled by $1000$ (milli-units).
- **First Solution Strategy**: `PATH_CHEAPEST_ARC`.
- **Local Search Metaheuristic**: `GUIDED_LOCAL_SEARCH` with time limit configured identically to the benchmark budget.
- **Dimensions**:
  - `Capacity`: Cumulative node demand $\le Q$.
  - `Time`: Cumulative travel time + waiting + service duration.
  - `Disjunction Penalty`: Set to $w_{\text{unserved}} \times 1000$ to penalize dropped customers.
  - `Fixed Cost`: $w_{\text{fixed}} \times 1000$ per vehicle used.

---

## 5. Solution Quality Comparison

On the committed benchmark instances (`tiny-8`, `small-20`, `medium-50`), Chronos demonstrates quality on par with OR-Tools:

- **tiny-8** (8 customers, 3 vehicles):
  - Chronos cost: $412.42$ ($0.046\text{ s}$)
  - OR-Tools cost: $412.42$ ($5.151\text{ s}$)
  - **Gap: 0.0%**
- **small-20** (20 customers, 5 vehicles):
  - Chronos cost: $631.426$ ($0.194\text{ s}$)
  - OR-Tools cost: $631.426$ ($15.206\text{ s}$)
  - **Gap: 0.0%**
- **medium-50** (50 customers, 10 vehicles):
  - Chronos cost: $1127.684$ ($1.250\text{ s}$)
  - OR-Tools cost: $1124.330$ ($30.180\text{ s}$)
  - **Gap: 0.298%** (within $0.3\%$ of OR-Tools while executing $24\times$ faster)

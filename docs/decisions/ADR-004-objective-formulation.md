# ADR-004: Objective Function Formulation and Soft/Hard Constraint Handling

## Status
Accepted

## Context
Real-world urban logistics balances multiple competing trade-offs: driver hours, vehicle dispatch overhead, fuel/distance, customer punctuality, and unserved deliveries. In practical operations, an infeasible schedule that drops one customer is preferable to total solver failure, provided unserved requests are heavily penalized and tracked.

## Decision
1. Formulate a 5-component configurable objective cost index:
   $$\text{Cost} = w_{\text{time}} \cdot T_{\text{travel}} + w_{\text{dist}} \cdot D_{\text{travel}} + w_{\text{fixed}} \cdot K_{\text{used}} + w_{\text{late}} \cdot \text{Lateness} + w_{\text{unserved}} \cdot |U|$$
2. Vehicle capacity ($Q$) and maximum route duration ($L$) are strictly enforced as **hard** constraints.
3. Time windows support two modes:
   - `hard`: Service must start on or before $l_i$ ($b_i \le l_i$). Any lateness renders the route infeasible.
   - `soft`: Service past $l_i$ is permitted but incurs linear lateness penalty ($w_{\text{late}} \cdot (b_i - l_i)$).
4. Partial solutions are formally valid if and only if all unassigned customers are explicitly documented in the unserved section with diagnostic reasons.

## Consequences
- **Positive**: Clear operational trade-offs; robust error reporting; matches industrial dispatching realities.
- **Negative**: Objective cost is dimensionless (a weighted index) rather than pure currency units, requiring users to tune penalty weights.

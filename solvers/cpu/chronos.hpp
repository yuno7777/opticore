// OptiCore custom CPU solver (codename "chronos").
//
// Pipeline (see docs/ALGORITHMS.md):
//   1. Clarke-Wright savings construction with exact feasibility checks.
//   2. Cheapest-feasible insertion for any leftover customers.
//   3. Feasibility-aware local search (2-opt intra-route, relocate, swap).
//   4. Seeded iterated-local-search perturbation (destroy + greedy repair).
//
// Deterministic: all stochastic choices draw from a single seed; all
// candidate enumeration follows deterministic orderings.
#pragma once

#include "opticore/solver.hpp"

namespace oc {

class ChronosSolver final : public Solver {
 public:
  const char* name() const override { return "chronos-cpu"; }
  Solution solve(const Scenario& sc, const SolverOptions& opts) override;
};

}  // namespace oc

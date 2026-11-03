// OptiCore independent solution validator.
//
// This is deliberately separate from any solver: it recomputes route
// feasibility and the objective directly from the raw scenario input and
// the route sequences, so it can validate solutions produced by ANY solver
// (custom C++ solver, OR-Tools baseline, hand-authored regression cases).
#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "opticore/objective.hpp"
#include "opticore/solution.hpp"
#include "opticore/types.hpp"

namespace oc {

struct Violation {
  std::string code;     // stable machine-readable code (see docs)
  std::string message;  // human-readable context
};

struct ValidationResult {
  bool ok = false;
  ObjectiveBreakdown recomputed;
  std::vector<RouteMetrics> metrics;   // one per non-empty route
  std::vector<Violation> violations;
  int routes_used = 0;
};

// Full validation pass. Never throws on validation failure; throws only for
// program misuse (null structures) or scenario-level catastrophic data.
ValidationResult validate_solution(const Scenario& sc, const Solution& sol);

// Shared tolerance for recomputed-vs-claimed objective comparison.
inline bool close_enough(double a, double b, double tol = 1e-6) {
  return std::fabs(a - b) <= tol * (1.0 + std::fabs(b));
}

}  // namespace oc

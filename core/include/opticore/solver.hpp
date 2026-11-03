// OptiCore solver interface. All solvers are deterministic given identical
// inputs and options: use the passed seed for every stochastic decision and
// deterministic orderings for all enumeration. Time budgets are the only
// nondeterministic stopping condition (documented trade-off: use
// max_iterations/time budgets equal across comparison runs).
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "opticore/solution.hpp"
#include "opticore/types.hpp"

namespace oc {

struct Scenario;

struct SolverOptions {
  uint64_t seed = 1;
  double time_budget_s = 0.0;  // 0 = no wall-clock budget (deterministic run)
  long long max_iterations = 0; // 0 = use solver default; bound on improvement loop
  int verbosity = 0;
};

class Solver {
 public:
  virtual ~Solver() = default;
  virtual const char* name() const = 0;
  // Returns a solution record; the caller must independently validate it.
  virtual Solution solve(const Scenario& sc, const SolverOptions& opts) = 0;
};

}  // namespace oc

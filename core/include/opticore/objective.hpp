// OptiCore objective evaluation over route sets.
#pragma once

#include "opticore/route_planner.hpp"
#include "opticore/types.hpp"

namespace oc {

struct Scenario;

struct EvaluatedSolution {
  ObjectiveBreakdown breakdown;
  std::vector<RouteMetrics> metrics;  // one per non-empty route, in order
};

// Compute the objective breakdown from raw route sequences (+ unserved
// count) using the Scenario's compact matrix representation.
EvaluatedSolution evaluate(const Scenario& sc,
                           const std::vector<std::vector<NodeId>>& routes,
                           int unserved_count);

double total_cost(const ObjectiveWeights& w, const ObjectiveBreakdown& b);

}  // namespace oc

// OptiCore route planner: forward scheduling pass over a route.
// Single source of truth for feasibility timing math, shared by the
// solver cost evaluation, the objective breakdown, and the validator.
#pragma once

#include "opticore/types.hpp"

namespace oc {

struct Scenario;
struct Solution;

// Forward scheduling pass over `stops` (customer ids only; depot endpoints
// implicit from Scenario). Uses only the scenario's travel-time matrix.
RouteMetrics plan_route(const Scenario& sc, const std::vector<NodeId>& stops);

}  // namespace oc

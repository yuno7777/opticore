#include "opticore/objective.hpp"

#include <algorithm>
#include <cmath>

#include "opticore/scenario.hpp"

namespace oc {

double total_cost(const ObjectiveWeights& w, const ObjectiveBreakdown& b) {
  return w.travel_time * b.travel_time_min + w.travel_dist * b.travel_dist_km +
         w.vehicle_fixed * static_cast<double>(b.vehicles_used) +
         w.lateness * b.total_lateness_min +
         w.unserved * static_cast<double>(b.unserved_count);
}

EvaluatedSolution evaluate(const Scenario& sc,
                           const std::vector<std::vector<NodeId>>& routes,
                           int unserved_count) {
  EvaluatedSolution out;
  ObjectiveBreakdown& b = out.breakdown;
  for (const auto& stops : routes) {
    if (stops.empty()) continue;
    RouteMetrics m = plan_route(sc, stops);
    out.metrics.push_back(m);
    b.travel_time_min += m.travel_time_min;
    b.travel_dist_km += m.travel_dist_km;
    b.total_lateness_min += m.lateness_min;
    b.vehicles_used += 1;
  }
  b.unserved_count = unserved_count;
  b.total_cost = total_cost(sc.weights, b);
  return out;
}

}  // namespace oc

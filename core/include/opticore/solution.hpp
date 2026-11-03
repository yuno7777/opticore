// OptiCore solution records + serialized .ocs file format.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "opticore/types.hpp"

namespace oc {

struct Solution {
  std::string scenario_name = "unnamed";
  std::string solver = "unknown";
  uint64_t seed = 0;
  SolverStatus status = SolverStatus::NotRun;
  TimeWindowMode time_window_mode = TimeWindowMode::Hard;

  // routes[vehicle_id] = ordered customer stops (no depot endpoints).
  // Vehicle ids >= routes.size() or empty vectors mean "vehicle unused".
  std::vector<std::vector<NodeId>> routes;

  // Unserved customers with an explicit, human-readable reason.
  std::vector<std::pair<NodeId, std::string>> unserved;

  // Breakdown as claimed by the producing solver (units per types.hpp).
  // The independent validator recomputes and must match within tolerance.
  ObjectiveBreakdown claimed;
};

Solution solution_from_text(const std::string& text);
Solution solution_from_file(const std::string& path);
std::string solution_to_text(const Solution& sol);

}  // namespace oc

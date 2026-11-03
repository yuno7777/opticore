// OptiCore scenario: immutable problem input + deterministic file format.
#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "opticore/types.hpp"

namespace oc {

// Thrown when scenario text (or any input file) cannot be parsed or
// violates structural sanity.
class ScenarioDataError : public std::runtime_error {
 public:
  explicit ScenarioDataError(const std::string& msg) : std::runtime_error(msg) {}
};

struct Scenario {
  std::string name = "unnamed";
  uint64_t seed = 0;
  std::string travel_model = "matrix";  // informational provenance label
  TimeWindowMode time_window_mode = TimeWindowMode::Hard;
  double vehicle_start_time = 0.0;  // all vehicles depart depot at this time
  int vehicle_count = 1;            // homogeneous fleet
  int capacity = 1;                 // load units per vehicle (hard constraint)
  double max_route_duration = -1.0; // minutes; negative disables
  ObjectiveWeights weights;

  std::vector<Node> nodes;  // nodes[0] == depot

  // Row-major N x N matrices (single-truth input; graph-free for MVP).
  std::vector<double> time_min;    // minutes
  std::vector<double> dist_km;     // kilometers

  int node_count() const { return static_cast<int>(nodes.size()); }
  double dist(int i, int j) const {
    return dist_km[i * node_count() + j];
  }
  double time(int i, int j) const {
    return time_min[i * node_count() + j];
  }
  bool is_customer_node(NodeId id) const {
    return id >= 1 && id < node_count() && nodes[static_cast<size_t>(id)].demand > 0;
  }
};

// Load or serialize the frozen version-1 text format.
// See docs/DATA_AND_CITY_MODEL.md for the grammar. Throws ScenarioDataError.
Scenario scenario_from_text(const std::string& text);
Scenario scenario_from_file(const std::string& path);
std::string scenario_to_text(const Scenario& sc);

// Structural sanity checks independent of any solution (units, ranges,
// matrix shape, depot conventions, non-finite values). Returns the list of
// human-readable problems; empty means the scenario is well-formed.
std::vector<std::string> scenario_sanity(const Scenario& sc);

}  // namespace oc

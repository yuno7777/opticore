#include <cstring>
#include <stdexcept>
#include <string>

#include "opticore/types.hpp"

namespace oc {

const char* solver_status_to_string(SolverStatus s) {
  switch (s) {
    case SolverStatus::NotRun: return "not_run";
    case SolverStatus::Feasible: return "feasible";
    case SolverStatus::Infeasible: return "infeasible";
    case SolverStatus::TimeoutIncumbent: return "timeout_incumbent";
    case SolverStatus::Failed: return "failed";
  }
  return "not_run";
}

SolverStatus solver_status_from_string(const std::string& s) {
  if (s == "feasible") return SolverStatus::Feasible;
  if (s == "infeasible") return SolverStatus::Infeasible;
  if (s == "timeout_incumbent") return SolverStatus::TimeoutIncumbent;
  if (s == "failed") return SolverStatus::Failed;
  return SolverStatus::NotRun;
}

const char* time_window_mode_to_string(TimeWindowMode m) {
  return m == TimeWindowMode::Hard ? "hard" : "soft";
}

bool time_window_mode_from_string(const std::string& s, TimeWindowMode* out) {
  if (s == "hard") { *out = TimeWindowMode::Hard; return true; }
  if (s == "soft") { *out = TimeWindowMode::Soft; return true; }
  return false;
}

}  // namespace oc

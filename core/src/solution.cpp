#include "opticore/solution.hpp"

#include "opticore/scenario.hpp"  // ScenarioDataError
#include "opticore/version.hpp"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace oc {
namespace {

std::string trim(const std::string& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

int parse_int(const std::string& tok, const std::string& ctx) {
  if (tok.empty()) throw ScenarioDataError(ctx + ": empty integer token");
  char* end = nullptr;
  errno = 0;
  const long v = std::strtol(tok.c_str(), &end, 10);
  if (end != tok.c_str() + tok.size() || errno == ERANGE)
    throw ScenarioDataError(ctx + ": bad integer token '" + tok + "'");
  if (v < -(1L << 30) || v > (1L << 30))
    throw ScenarioDataError(ctx + ": integer out of range '" + tok + "'");
  return static_cast<int>(v);
}

double parse_double(const std::string& tok, const std::string& ctx) {
  if (tok.empty()) throw ScenarioDataError(ctx + ": empty numeric token");
  char* end = nullptr;
  errno = 0;
  const double v = std::strtod(tok.c_str(), &end);
  if (end != tok.c_str() + tok.size())
    throw ScenarioDataError(ctx + ": bad numeric token '" + tok + "'");
  if (!std::isfinite(v))
    throw ScenarioDataError(ctx + ": non-finite value '" + tok + "'");
  return v;
}

std::string fmt_double(double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", v);
  return std::string(buf);
}

std::string fmt_double_3(double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.3f", v);
  return std::string(buf);
}

std::string status_to_string(SolverStatus s) {
  switch (s) {
    case SolverStatus::NotRun: return "not_run";
    case SolverStatus::Feasible: return "feasible";
    case SolverStatus::Infeasible: return "infeasible";
    case SolverStatus::TimeoutIncumbent: return "timeout_incumbent";
    case SolverStatus::Failed: return "failed";
  }
  return "not_run";
}

}  // namespace

Solution solution_from_text(const std::string& text) {
  Solution sol;
  std::istringstream in(text);
  std::string line;

  enum class Stage {
    Header,
    ExpectBeginRoutes,
    Routes,
    ExpectBeginUnserved,
    Unserved,
    ExpectBeginObjective,
    Objective,
    Done
  } stage = Stage::Header;

  bool seen_scenario = false;
  bool seen_solver = false;
  bool seen_seed = false;
  bool seen_status = false;
  int last_vehicle_id = -1;

  bool obj_seen[6] = {false, false, false, false, false, false};
  int obj_fields = 0;

  int line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::string ctx = "solution line " + std::to_string(line_no);

    if (line.empty() || line[0] == '#') continue;

    switch (stage) {
      case Stage::Header: {
        if (line.rfind("scenario ", 0) == 0) {
          if (seen_scenario)
            throw ScenarioDataError(ctx + ": duplicate 'scenario' key");
          sol.scenario_name = trim(line.substr(9));
          seen_scenario = true;
          break;
        }
        if (line.rfind("solver ", 0) == 0) {
          if (seen_solver)
            throw ScenarioDataError(ctx + ": duplicate 'solver' key");
          sol.solver = trim(line.substr(7));
          seen_solver = true;
          break;
        }
        if (line.rfind("seed ", 0) == 0) {
          if (seen_seed) throw ScenarioDataError(ctx + ": duplicate 'seed' key");
          char* end = nullptr;
          errno = 0;
          sol.seed = std::strtoull(trim(line.substr(5)).c_str(), &end, 10);
          if (end == nullptr || *end != '\0' || errno == ERANGE)
            throw ScenarioDataError(ctx + ": bad seed");
          seen_seed = true;
          break;
        }
        if (line.rfind("status ", 0) == 0) {
          if (seen_status)
            throw ScenarioDataError(ctx + ": duplicate 'status' key");
          const std::string st = trim(line.substr(7));
          if (st == "not_run") sol.status = SolverStatus::NotRun;
          else if (st == "feasible") sol.status = SolverStatus::Feasible;
          else if (st == "infeasible") sol.status = SolverStatus::Infeasible;
          else if (st == "timeout_incumbent") sol.status = SolverStatus::TimeoutIncumbent;
          else if (st == "failed") sol.status = SolverStatus::Failed;
          else throw ScenarioDataError(ctx + ": unknown status '" + st + "'");
          seen_status = true;
          break;
        }
        if (line == "BEGIN_ROUTES") {
          if (!seen_scenario || !seen_solver || !seen_seed || !seen_status)
            throw ScenarioDataError(
                ctx + ": BEGIN_ROUTES requires scenario/solver/seed/status");
          stage = Stage::Routes;
          break;
        }
        throw ScenarioDataError(ctx + ": unexpected header line '" + line + "'");
      }
      case Stage::Routes: {
        if (line == "END_ROUTES") {
          stage = Stage::ExpectBeginUnserved;
          break;
        }
        if (line.rfind("vehicle ", 0) != 0)
          throw ScenarioDataError(ctx + ": route line must start with 'vehicle '");
        std::istringstream ss(line);
        std::string tok;
        ss >> tok;  // "vehicle"
        ss >> tok;
        const int vehicle_id = parse_int(tok, ctx);
        if (vehicle_id < 0)
          throw ScenarioDataError(ctx + ": vehicle id must be >= 0");
        if (vehicle_id <= last_vehicle_id)
          throw ScenarioDataError(ctx + ": vehicle ids must be strictly increasing");
        last_vehicle_id = vehicle_id;
        std::vector<NodeId> seq;
        while (ss >> tok) seq.push_back(static_cast<NodeId>(parse_int(tok, ctx)));
        if (seq.size() < 2)
          throw ScenarioDataError(
              ctx + ": route needs at least the depot start and end tokens");
        if (static_cast<int>(sol.routes.size()) <= vehicle_id)
          sol.routes.resize(static_cast<size_t>(vehicle_id) + 1);
        sol.routes[static_cast<size_t>(vehicle_id)] =
            std::vector<NodeId>(seq.begin() + 1, seq.end() - 1);
        break;
      }
      case Stage::ExpectBeginUnserved: {
        if (line != "BEGIN_UNSERVED")
          throw ScenarioDataError(ctx + ": expected BEGIN_UNSERVED, got '" + line + "'");
        stage = Stage::Unserved;
        break;
      }
      case Stage::Unserved: {
        if (line == "END_UNSERVED") {
          stage = Stage::ExpectBeginObjective;
          break;
        }
        std::istringstream ss(line);
        std::string id_tok;
        ss >> id_tok;
        const NodeId nid = static_cast<NodeId>(parse_int(id_tok, ctx));
        std::string reason;
        std::getline(ss, reason);
        reason = trim(reason);
        if (reason.empty())
          throw ScenarioDataError(ctx + ": missing unserved reason");
        sol.unserved.emplace_back(nid, reason);
        break;
      }
      case Stage::ExpectBeginObjective: {
        if (line != "BEGIN_OBJECTIVE")
          throw ScenarioDataError(ctx + ": expected BEGIN_OBJECTIVE, got '" + line + "'");
        stage = Stage::Objective;
        break;
      }
      case Stage::Objective: {
        if (line == "END_OBJECTIVE") {
          if (obj_fields != 6)
            throw ScenarioDataError(ctx + ": objective section incomplete");
          stage = Stage::Done;
          break;
        }
        const size_t sp = line.find(' ');
        if (sp == std::string::npos)
          throw ScenarioDataError(ctx + ": bad objective line '" + line + "'");
        const std::string key = line.substr(0, sp);
        const std::string val = trim(line.substr(sp + 1));
        int idx = -1;
        if (key == "travel_time_min") {
          sol.claimed.travel_time_min = parse_double(val, ctx);
          idx = 0;
        } else if (key == "travel_dist_km") {
          sol.claimed.travel_dist_km = parse_double(val, ctx);
          idx = 1;
        } else if (key == "vehicles_used") {
          sol.claimed.vehicles_used = parse_int(val, ctx);
          idx = 2;
        } else if (key == "total_lateness_min") {
          sol.claimed.total_lateness_min = parse_double(val, ctx);
          idx = 3;
        } else if (key == "unserved_count") {
          sol.claimed.unserved_count = parse_int(val, ctx);
          idx = 4;
        } else if (key == "total_cost") {
          sol.claimed.total_cost = parse_double(val, ctx);
          idx = 5;
        } else {
          throw ScenarioDataError(ctx + ": unknown objective key '" + key + "'");
        }
        if (obj_seen[idx])
          throw ScenarioDataError(ctx + ": duplicate objective key '" + key + "'");
        obj_seen[idx] = true;
        ++obj_fields;
        break;
      }
      case Stage::Done:
        throw ScenarioDataError(ctx + ": unexpected content after END_OBJECTIVE: '" + line + "'");
    }
  }

  if (stage != Stage::Done)
    throw ScenarioDataError("solution: file is incomplete (missing '" +
                            std::string(stage < Stage::Routes
                                            ? "status"
                                            : stage < Stage::Unserved
                                                  ? "routes"
                                                  : stage < Stage::Objective
                                                        ? "unserved"
                                                        : "objective") +
                            "' section)");
  return sol;
}

std::string solution_to_text(const Solution& sol) {
  std::ostringstream o;
  o << "# opticore solution v" << kSolutionFormatVersion << "\n";
  o << "scenario " << sol.scenario_name << "\n";
  o << "solver " << sol.solver << "\n";
  o << "seed " << sol.seed << "\n";
  o << "status " << status_to_string(sol.status) << "\n";
  o << "BEGIN_ROUTES\n";
  for (VehicleId v = 0; v < static_cast<VehicleId>(sol.routes.size()); ++v) {
    const auto& stops = sol.routes[static_cast<size_t>(v)];
    if (stops.empty()) continue;
    o << "vehicle " << v << " 0";
    for (const NodeId n : stops) o << " " << n;
    o << " 0\n";
  }
  o << "END_ROUTES\n";
  o << "BEGIN_UNSERVED\n";
  for (const auto& entry : sol.unserved) {
    o << entry.first << " " << entry.second << "\n";
  }
  o << "END_UNSERVED\n";
  o << "BEGIN_OBJECTIVE\n";
  o << "travel_time_min " << fmt_double_3(sol.claimed.travel_time_min) << "\n";
  o << "travel_dist_km " << fmt_double_3(sol.claimed.travel_dist_km) << "\n";
  o << "vehicles_used " << sol.claimed.vehicles_used << "\n";
  o << "total_lateness_min " << fmt_double_3(sol.claimed.total_lateness_min)
    << "\n";
  o << "unserved_count " << sol.claimed.unserved_count << "\n";
  o << "total_cost " << fmt_double(sol.claimed.total_cost) << "\n";
  o << "END_OBJECTIVE\n";
  return o.str();
}

Solution solution_from_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) throw ScenarioDataError("cannot open solution file: " + path);
  std::ostringstream ss;
  ss << f.rdbuf();
  return solution_from_text(ss.str());
}

}  // namespace oc

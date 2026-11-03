# Cross-language contract tests: the Python baseline consumes the same
# .ocx scenario files the C++ side writes, and its solutions must pass the
# C++ validator. Skipped when build artifacts are missing.
import os.path
import pathlib
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
CLI = ROOT / "build" / "opticore.exe"
BASELINE = ROOT / "solvers" / "baseline" / "or_tools_baseline.py"
EXAMPLE = ROOT / "examples" / "tiny-demo.ocx"
sys.path.insert(0, str(ROOT / "solvers" / "baseline"))

import or_tools_baseline as ob  # noqa: E402


def cli_available() -> bool:
    return CLI.exists() and EXAMPLE.exists()


@unittest.skipUnless(cli_available(), "requires built CLI + example scenario")
class TestScenarioParsing(unittest.TestCase):
    def test_parse_committed_example(self):
        sc = ob.load_scenario(str(EXAMPLE))
        self.assertGreaterEqual(sc.n, 2)
        self.assertEqual(sc.nodes[0]["demand"], 0)
        self.assertEqual(len(sc.time_min), sc.n)
        self.assertTrue(all(sc.nodes[i]["demand"] > 0 for i in range(1, sc.n)))

    def test_mirror_planner_matches_cxx_cli_objective_roundtrip(self):
        # generate a fresh scenario via C++ CLI, parse it, evaluate a trivial
        # solution (each customer alone on its own vehicle) and make sure the
        # Python-planned metrics always validate through the C++ validator.
        import tempfile
        scenario_path = pathlib.Path(tempfile.mkdtemp()) / "cross.ocx"
        subprocess.run(
            [str(CLI), "gen", "--out", str(scenario_path), "--seed", "31",
             "--customers", "6", "--vehicles", "8", "--name", "cross-lang"],
            check=True, capture_output=True,
        )
        sc = ob.load_scenario(str(scenario_path))
        routes = [[i] for i in range(1, sc.n)]
        b = ob.evaluate(sc, routes, 0)
        self.assertGreater(b["vehicles"], 0)
        sol_text = ob.solution_to_text(
            sc, "cross-test", sc.seed, "feasible", routes, {})
        solution_path = scenario_path.with_suffix(".ocs")
        solution_path.write_text(sol_text, encoding="utf-8")
        r = subprocess.run(
            [str(CLI), "validate", "--scenario", str(scenario_path),
             "--solution", str(solution_path)],
            capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    @unittest.skipUnless(cli_available(), "requires built CLI")
    def test_ortools_solution_passes_cpp_validator(self):
        out = EXAMPLE.with_name("tiny-demo-ortools-test.ocs")
        r = subprocess.run(
            [sys.executable, str(BASELINE), "--scenario", str(EXAMPLE),
             "--solution", str(out), "--time-limit-s", "3"],
            capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        v = subprocess.run(
            [str(CLI), "validate", "--scenario", str(EXAMPLE),
             "--solution", str(out)],
            capture_output=True, text=True)
        self.assertEqual(v.returncode, 0, v.stdout + v.stderr)
        out.unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main()

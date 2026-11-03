# ADR-002: Zero-Dependency C++ Core and Bespoke Text Formats

## Status
Accepted

## Context
Standard vehicle routing problems require exchanging scenarios (depot, customer locations, demands, time windows, distance/time matrices) and solution routes across different components (solvers, validators, baselines, analysis scripts).

Common serialization formats like JSON, YAML, or Protocol Buffers introduce significant dependency overhead. C++ JSON libraries (e.g. nlohmann/json) add compilation time, and Protocol Buffers requires compiler toolchains and runtime libraries. Furthermore, reading massive 2D floating-point matrices in JSON is computationally slow and memory-inefficient.

## Decision
1. Keep `opticore_core` completely free of third-party dependencies, relying exclusively on C++20 standard library facilities.
2. Define two line-oriented, human-readable text formats:
   - `.ocx` (OptiCore Scenario format v1)
   - `.ocs` (OptiCore Solution format v1)
3. Implement lightweight, deterministic parsers and serializers in both C++ (`core/src/scenario.cpp`, `core/src/solution.cpp`) and Python (`solvers/baseline/or_tools_baseline.py`).

## Consequences
- **Positive**: Blazing fast compilation; zero package manager dependencies (no vcpkg, conan); transparent git diffs of benchmark instances; fast stream-based matrix parsing.
- **Negative**: Formats are custom to OptiCore; third-party tools require adapters or the OptiCore CLI to ingest them.

# SECURITY.md — Security Policy and Threat Modeling

This document outlines the security architecture, threat model, input validation guarantees, and vulnerability reporting procedures for OptiCore.

---

## 1. Threat Model and Attack Surface

OptiCore operates as a computational optimization engine. In its current and planned deployments, potential threat vectors include:

1. **Malicious Scenario Files (`.ocx`)**: Crafted inputs attempting buffer overflows, negative indices, infinite loops, or denial of service via memory exhaustion.
2. **Path Traversal Attacks**: Malicious input paths passed to `--scenario`, `--solution`, or future REST endpoints attempting to access or overwrite host files.
3. **Denial of Service (DoS)**: Workloads specifying millions of nodes or extreme ILS iteration limits intended to exhaust CPU/RAM resources.
4. **Third-Party Dependency Vulnerabilities**: Supply-chain risks in external packages.

---

## 2. Defensive Controls and Mitigations

### 2.1. Strict Input Validation (`core/src/scenario.cpp`)
- **Range & Value Bounds**: Node indices, demands ($q_i \ge 0$), vehicle capacities ($Q > 0$), and coordinates are strictly checked during parsing. Negative demand or non-positive capacity causes immediate parse rejection.
- **Matrix Shape Invariants**: Matrix dimensions must strictly match $N \times N$, where $N$ matches the declared `NODES` header. Mismatches trigger descriptive parse errors rather than out-of-bounds memory accesses.
- **Header Grammar State Machine**: Format headers require specific order. Missing tokens or unexpected tokens immediately abort processing without dynamic memory leaks.

### 2.2. Memory Safety and Bounds Checking
- **Modern C++20**: Standard containers (`std::vector`, `std::string`, `std::span`) manage dynamic memory. No manual raw-pointer `malloc`/`free` calls exist in `opticore_core`.
- **Zero-Dependency Core**: The C++ library links against standard libc/libstdc++ only. No vulnerable image, JSON, XML, or networking libraries are linked into the engine core binary.

### 2.3. Path Safety & Execution Isolation
- Solvers never invoke external shell scripts or evaluate dynamic user code.
- File writes are restricted to target output paths provided on the CLI or generated within designated sandbox directories.

---

## 3. Vulnerability Reporting

If you identify a security issue or vulnerability in OptiCore, please report it directly to the repository maintainer rather than opening a public issue. Include:
1. Scenario input or reproduction steps triggering the vulnerability.
2. Target operating system, compiler, and binary build flags.
3. Impact assessment (e.g., crash, memory leak, validation bypass).

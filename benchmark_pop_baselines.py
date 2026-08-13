#!/usr/bin/env python3

"""Run the upstream POP-S-B and POPH-S-B encodings under a controlled BCP protocol.

The encoding classes are imported unchanged from the supplementary source of Faber et al.
This adapter makes the descending linear search, timing boundary, seed, and result metadata
explicit so the baselines can be compared with the proposed implementations.
"""

import argparse
import csv
import datetime
import hashlib
import importlib
import math
import os
import platform
import shlex
import subprocess
import sys
import tempfile
import time
import types
from pathlib import Path

import networkx as nx
import numpy as np


UPSTREAM_URL = "https://github.com/s6dafabe/popsatgcpbcp.git"
UPSTREAM_SHA = "8f19dbff4135e6cff9e4b147ebe8462603d5fe03"
SUPPORTED_METHODS = {
    "POP-S-B": "POP_SAT_BCP",
    "POPH-S-B": "POPHyb_SAT_BCP",
}

RESULT_COLUMNS = [
    "name", "V", "E", "upper_bound", "variables", "clauses", "cnf_calls", "solver_calls",
    "status", "span", "encoding_time", "total_solving_time", "time_used",
    "timed_out", "optimality_proven", "run_id", "source_sha", "source_dirty",
    "upstream_source_url", "upstream_source_sha", "upstream_source_dirty",
    "input_sha256", "binary_sha256", "command", "host", "platform",
    "runner_versions", "concurrency", "recorded_at", "solver", "solver_seed",
    "method", "time_limit", "search_strategy",
]


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_output(repo_dir, *args):
    return subprocess.run(
        ["git", "-C", str(repo_dir), *args], capture_output=True, text=True, check=True
    ).stdout.strip()


def validate_upstream_source(source_dir, expected_sha=UPSTREAM_SHA):
    source_dir = Path(source_dir).resolve()
    required = [
        source_dir / "source" / "ModelsSAT.py",
        source_dir / "source" / "LogicFormula.py",
        source_dir / "source" / "SatParser.py",
    ]
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise FileNotFoundError(f"Incomplete upstream source checkout; missing {missing}")

    actual_sha = git_output(source_dir, "rev-parse", "HEAD")
    if actual_sha != expected_sha:
        raise RuntimeError(
            f"Expected upstream baseline SHA {expected_sha}, found {actual_sha}"
        )
    dirty = bool(git_output(source_dir, "status", "--porcelain", "--untracked-files=all"))
    if dirty:
        raise RuntimeError("Refusing to benchmark a dirty upstream baseline checkout")
    return source_dir, actual_sha, dirty


def load_upstream_models(source_dir):
    """Load only the original SAT encodings, without the Gurobi-dependent runners.

    ModelsSAT imports Preprocessing for optional GCP clique precoloring. Neither BCP baseline
    uses it, so a guard module prevents the unrelated Gurobi dependency and raises if that
    code path is ever reached.
    """

    # Preserve the pinned checkout byte-for-byte across repeated benchmark invocations.
    sys.dont_write_bytecode = True
    source_path = str(Path(source_dir) / "source")
    if source_path not in sys.path:
        sys.path.insert(0, source_path)

    guard = types.ModuleType("Preprocessing")

    def reject_precoloring(*_args, **_kwargs):
        raise RuntimeError("BCP baseline unexpectedly requested clique precoloring")

    guard.precolorCliqueSAT = reject_precoloring
    sys.modules["Preprocessing"] = guard

    for module_name in ("LogicFormula", "SatParser", "ModelsSAT"):
        sys.modules.pop(module_name, None)
    return importlib.import_module("ModelsSAT")


def read_bcp_graph(path):
    graph = None
    declared_edges = None
    parsed_edges = 0
    edge_pairs = set()
    with open(path, encoding="utf-8") as handle:
        for line_number, raw_line in enumerate(handle, start=1):
            line = raw_line.strip()
            if not line or line.startswith("c"):
                continue
            fields = line.split()
            if fields[0] == "p":
                if graph is not None or len(fields) != 4 or fields[1] != "band":
                    raise ValueError(f"Invalid BCP header at {path}:{line_number}")
                vertices, declared_edges = map(int, fields[2:])
                if vertices < 0 or declared_edges < 0:
                    raise ValueError(f"Negative BCP dimensions at {path}:{line_number}")
                graph = nx.empty_graph(vertices)
            elif fields[0] == "e":
                if graph is None:
                    raise ValueError(f"Edge encountered before problem line in {path}")
                if len(fields) != 4:
                    raise ValueError(f"BCP edge must have a weight in {path}: {line}")
                u, v, weight = map(int, fields[1:])
                parsed_edges += 1
                if not 1 <= u <= graph.number_of_nodes() or not 1 <= v <= graph.number_of_nodes():
                    raise ValueError(f"Edge endpoint out of range at {path}:{line_number}")
                if weight <= 0:
                    raise ValueError(f"Nonpositive edge weight at {path}:{line_number}")
                edge_pair = tuple(sorted((u, v)))
                if edge_pair in edge_pairs:
                    raise ValueError(f"Duplicate edge at {path}:{line_number}")
                edge_pairs.add(edge_pair)
                if u != v:
                    graph.add_edge(u - 1, v - 1, weight=weight)
            elif fields[0] == "n":
                # Multicoloring demands are outside the BCP projection used by the manuscript.
                if graph is None or len(fields) != 3:
                    raise ValueError(f"Invalid demand record at {path}:{line_number}")
                vertex, demand = map(int, fields[1:])
                if not 1 <= vertex <= graph.number_of_nodes() or demand <= 0:
                    raise ValueError(f"Invalid demand record at {path}:{line_number}")
                continue
            else:
                raise ValueError(f"Unknown record type at {path}:{line_number}")
    if graph is None:
        raise ValueError(f"Missing DIMACS problem line in {path}")
    if parsed_edges != declared_edges:
        raise ValueError(
            f"Header declares {declared_edges} edges but {parsed_edges} were read from {path}"
        )
    return graph


def greedy_upper_bound(graph):
    """Return the positive-domain upper bound used by the proposed implementations."""

    n = graph.number_of_nodes()
    if n == 0:
        return 0

    colors = [-1] * n
    pending = []
    max_color = 0

    def start_vertex():
        best = -1
        best_degree = -1
        for vertex in range(n):
            if colors[vertex] == -1 and graph.degree(vertex) > best_degree:
                best = vertex
                best_degree = graph.degree(vertex)
        return best

    import heapq

    while any(color == -1 for color in colors):
        heapq.heappush(pending, start_vertex())
        while pending:
            vertex = heapq.heappop(pending)
            if colors[vertex] != -1:
                continue
            colors[vertex] = 0
            intervals = sorted(
                (colors[neighbor] - data["weight"], colors[neighbor] + data["weight"])
                for neighbor, data in graph[vertex].items()
                if colors[neighbor] != -1
            )
            index = 0
            while index < len(intervals) and colors[vertex] > intervals[index][0]:
                colors[vertex] = max(colors[vertex], intervals[index][1])
                index += 1
            max_color = max(max_color, colors[vertex])
            for neighbor in graph.neighbors(vertex):
                if colors[neighbor] == -1:
                    heapq.heappush(pending, neighbor)

    return max_color + 1


def parse_dimacs_result(stdout):
    status = None
    assignments = {}
    for raw_line in stdout.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        fields = line.split()
        if fields[0] == "s" and len(fields) >= 2:
            if fields[1] == "SATISFIABLE":
                status = "SATISFIABLE"
            elif fields[1] == "UNSATISFIABLE":
                status = "UNSATISFIABLE"
            elif fields[1] == "UNKNOWN":
                status = "UNKNOWN"
        elif fields[0] == "v":
            for token in fields[1:]:
                literal = int(token)
                if literal:
                    assignments[abs(literal)] = 1 if literal > 0 else 0

    if status == "SATISFIABLE":
        if not assignments:
            raise RuntimeError("SAT solver returned SATISFIABLE without a model")
        values = [0] * (max(assignments) + 1)
        for variable, value in assignments.items():
            values[variable] = value
        return status, values
    return status, None


def validate_coloring(graph, color_groups):
    assigned = {}
    for color, vertices in color_groups.items():
        for vertex in vertices:
            if vertex in assigned:
                raise RuntimeError(f"Vertex {vertex} has more than one decoded color")
            assigned[vertex] = color
    if len(assigned) != graph.number_of_nodes():
        raise RuntimeError("Decoded SAT model does not color every vertex")
    for u, v, data in graph.edges(data=True):
        if abs(assigned[u] - assigned[v]) < data["weight"]:
            raise RuntimeError(f"Invalid decoded coloring on edge ({u}, {v})")


def read_cnf_header(path):
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("p cnf "):
                _, _, variables, clauses = line.split()
                return int(variables), int(clauses)
    raise RuntimeError(f"Missing CNF header in {path}")


def run_solver(solver_path, cnf_path, seed, timeout):
    command = [str(solver_path), f"--seed={seed}", str(cnf_path)]
    started = time.perf_counter()
    try:
        completed = subprocess.run(
            command, capture_output=True, text=True, timeout=timeout, check=False
        )
    except subprocess.TimeoutExpired:
        return "UNKNOWN", None, time.perf_counter() - started

    elapsed = time.perf_counter() - started
    status, assignment = parse_dimacs_result(completed.stdout)
    expected_codes = {"SATISFIABLE": 10, "UNSATISFIABLE": 20, "UNKNOWN": 0}
    if status is None or completed.returncode != expected_codes.get(status, completed.returncode):
        raise RuntimeError(
            f"Unexpected solver result (exit={completed.returncode}, status={status}): "
            f"{completed.stderr.strip()}"
        )
    return status, assignment, elapsed


def solve_instance(models, method, graph, solver_path, seed, time_limit):
    model = getattr(models, SUPPORTED_METHODS[method])()
    upper_bound = greedy_upper_bound(graph)
    if upper_bound == 0:
        return {
            "upper_bound": 0, "variables": 0, "clauses": 0, "cnf_calls": 0,
            "solver_calls": 0,
            "status": "OPTIMAL", "span": 0, "encoding_time": 0.0,
            "total_solving_time": 0.0, "time_used": 0.0, "timed_out": False,
            "optimality_proven": True,
        }

    encoding_time = 0.0
    solving_time = 0.0
    maximum_variables = 0
    maximum_clauses = 0
    cnf_calls = 0
    solver_calls = 0
    last_feasible_span = None

    with tempfile.TemporaryDirectory(prefix="bcp-pop-baseline-") as temp_dir:
        cnf_path = Path(temp_dir) / "decision.cnf"
        for candidate in range(upper_bound, 0, -1):
            encoding_started = time.perf_counter()
            model.k_coloring_formula(graph, candidate, output=str(cnf_path))
            encoding_time += time.perf_counter() - encoding_started
            cnf_calls += 1
            variables, clauses = read_cnf_header(cnf_path)
            maximum_variables = max(maximum_variables, variables)
            maximum_clauses = max(maximum_clauses, clauses)

            remaining = time_limit - encoding_time - solving_time
            if remaining <= 0:
                status = "SATISFIABLE" if last_feasible_span is not None else "UNKNOWN"
                return {
                    "upper_bound": upper_bound, "variables": maximum_variables,
                    "clauses": maximum_clauses, "cnf_calls": cnf_calls,
                    "solver_calls": solver_calls,
                    "status": status, "span": last_feasible_span if last_feasible_span is not None else -1,
                    "encoding_time": encoding_time, "total_solving_time": solving_time,
                    "time_used": encoding_time + solving_time, "timed_out": True,
                    "optimality_proven": False,
                }

            sat_status, assignment, elapsed = run_solver(
                solver_path, cnf_path, seed, remaining
            )
            solver_calls += 1
            solving_time += elapsed

            if sat_status == "SATISFIABLE":
                coloring = model.coloring_from_vars(assignment)
                validate_coloring(graph, coloring)
                last_feasible_span = candidate
                continue

            if sat_status == "UNSATISFIABLE":
                if last_feasible_span is None:
                    raise RuntimeError(
                        f"Greedy upper bound {upper_bound} was not feasible for {method}"
                    )
                return {
                    "upper_bound": upper_bound, "variables": maximum_variables,
                    "clauses": maximum_clauses, "cnf_calls": cnf_calls,
                    "solver_calls": solver_calls,
                    "status": "OPTIMAL", "span": last_feasible_span,
                    "encoding_time": encoding_time, "total_solving_time": solving_time,
                    "time_used": encoding_time + solving_time, "timed_out": False,
                    "optimality_proven": True,
                }

            status = "SATISFIABLE" if last_feasible_span is not None else "UNKNOWN"
            return {
                "upper_bound": upper_bound, "variables": maximum_variables,
                "clauses": maximum_clauses, "cnf_calls": cnf_calls,
                "solver_calls": solver_calls,
                "status": status, "span": last_feasible_span if last_feasible_span is not None else -1,
                "encoding_time": encoding_time, "total_solving_time": solving_time,
                "time_used": encoding_time + solving_time, "timed_out": True,
                "optimality_proven": False,
            }

    # The candidate value one must be feasible only for an edgeless graph.
    if last_feasible_span == 1:
        return {
            "upper_bound": upper_bound, "variables": maximum_variables,
            "clauses": maximum_clauses, "cnf_calls": cnf_calls, "status": "OPTIMAL",
            "solver_calls": solver_calls,
            "span": 1, "encoding_time": encoding_time,
            "total_solving_time": solving_time, "time_used": encoding_time + solving_time,
            "timed_out": False, "optimality_proven": True,
        }
    raise RuntimeError("Descending search ended without proving an optimum")


def load_existing_rows(output_path):
    if not output_path.exists():
        return []
    with output_path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        rows = list(reader)
    if reader.fieldnames != RESULT_COLUMNS:
        raise RuntimeError(
            f"Cannot resume {output_path}: result columns do not match this runner"
        )
    return rows


def validate_existing_rows(rows, expected, input_hashes, repetitions):
    """Reject a partial CSV from any different machine, binary, or protocol."""

    completed = set()
    for row_number, row in enumerate(rows, start=2):
        name = row["name"]
        try:
            run_id = int(row["run_id"])
        except ValueError as error:
            raise RuntimeError(
                f"Cannot resume row {row_number}: invalid run_id {row['run_id']!r}"
            ) from error
        key = (name, run_id)
        if key in completed:
            raise RuntimeError(f"Cannot resume: duplicate result {key}")
        if name not in input_hashes or not 1 <= run_id <= repetitions:
            raise RuntimeError(f"Cannot resume row {row_number}: unexpected result {key}")
        if row["input_sha256"] != input_hashes[name]:
            raise RuntimeError(f"Cannot resume row {row_number}: input hash changed for {name}")
        for column, wanted in expected.items():
            if row[column] != str(wanted):
                raise RuntimeError(
                    f"Cannot resume row {row_number}: {column} is {row[column]!r}, "
                    f"expected {str(wanted)!r}"
                )
        encoding = float(row["encoding_time"])
        solving = float(row["total_solving_time"])
        total = float(row["time_used"])
        if not all(math.isfinite(value) and value >= 0 for value in (encoding, solving, total)):
            raise RuntimeError(f"Cannot resume row {row_number}: invalid timing value")
        if abs(total - encoding - solving) > 1e-9:
            raise RuntimeError(
                f"Cannot resume row {row_number}: time_used is not encoding plus solving"
            )
        completed.add(key)
    return completed


def write_rows(output_path, rows):
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_suffix(output_path.suffix + ".tmp")
    with temporary.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=RESULT_COLUMNS)
        writer.writeheader()
        writer.writerows(rows)
    os.replace(temporary, output_path)


def main():
    parser = argparse.ArgumentParser(
        description="Benchmark the original POP-S-B/POPH-S-B encodings with descending search."
    )
    parser.add_argument("method", choices=sorted(SUPPORTED_METHODS))
    parser.add_argument("--source_dir", required=True, type=Path)
    parser.add_argument("--dataset_dir", required=True, type=Path)
    parser.add_argument("--solver", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--time_limit", type=float, default=3600.0)
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--solver_seed", type=int, default=0)
    parser.add_argument("--instance", action="append", default=[])
    args = parser.parse_args()

    if args.time_limit <= 0:
        parser.error("--time_limit must be positive")
    if args.repetitions < 1:
        parser.error("--repetitions must be at least one")
    if not args.solver.is_file() or not os.access(args.solver, os.X_OK):
        parser.error(f"CaDiCaL executable not found or not executable: {args.solver}")

    version = subprocess.run(
        [str(args.solver.resolve()), "--version"], capture_output=True, text=True, check=True
    ).stdout.strip()
    if version != "1.9.5":
        parser.error(f"Expected CaDiCaL 1.9.5, found {version!r}")

    source_dir, upstream_sha, upstream_dirty = validate_upstream_source(args.source_dir)
    models = load_upstream_models(source_dir)

    repo_root = Path(__file__).resolve().parent
    source_sha = git_output(repo_root, "rev-parse", "HEAD")
    source_dirty = bool(git_output(
        repo_root, "status", "--porcelain", "--untracked-files=all", "--", ".",
        ":(exclude)result/**", ":(exclude)external/popsatgcpbcp/source/**"
    ))
    solver_path = args.solver.resolve()
    solver_hash = sha256_file(solver_path)

    selected = set(args.instance)
    files = sorted(path for path in args.dataset_dir.glob("*.col") if not selected or path.name in selected)
    if selected - {path.name for path in files}:
        parser.error(f"Unknown requested instances: {sorted(selected - {path.name for path in files})}")
    if not files:
        parser.error("No .col input files selected")

    runner_versions = (
        f"Python {platform.python_version()}; networkx {nx.__version__}; numpy {np.__version__}"
    )
    input_hashes = {path.name: sha256_file(path) for path in files}
    expected_resume_metadata = {
        "source_sha": source_sha,
        "source_dirty": source_dirty,
        "upstream_source_url": UPSTREAM_URL,
        "upstream_source_sha": upstream_sha,
        "upstream_source_dirty": upstream_dirty,
        "binary_sha256": solver_hash,
        "host": platform.node(),
        "platform": platform.platform(),
        "runner_versions": runner_versions,
        "concurrency": 1,
        "solver": "CaDiCaL 1.9.5",
        "solver_seed": args.solver_seed,
        "method": args.method,
        "time_limit": args.time_limit,
        "search_strategy": "descending-linear",
    }
    rows = load_existing_rows(args.output)
    completed = validate_existing_rows(
        rows, expected_resume_metadata, input_hashes, args.repetitions
    )

    for run_id in range(1, args.repetitions + 1):
        for input_path in files:
            if (input_path.name, run_id) in completed:
                continue
            print(f"[{args.method}] run {run_id}/{args.repetitions}: {input_path.name}", flush=True)
            graph = read_bcp_graph(input_path)
            result = solve_instance(
                models, args.method, graph, solver_path, args.solver_seed, args.time_limit
            )
            row = {
                "name": input_path.name,
                "V": graph.number_of_nodes(),
                "E": graph.number_of_edges(),
                "upper_bound": result["upper_bound"],
                "variables": result["variables"],
                "clauses": result["clauses"],
                "cnf_calls": result["cnf_calls"],
                "solver_calls": result["solver_calls"],
                "status": result["status"],
                "span": result["span"],
                "encoding_time": result["encoding_time"],
                "total_solving_time": result["total_solving_time"],
                "time_used": result["time_used"],
                "timed_out": result["timed_out"],
                "optimality_proven": result["optimality_proven"],
                "run_id": run_id,
                "source_sha": source_sha,
                "source_dirty": source_dirty,
                "upstream_source_url": UPSTREAM_URL,
                "upstream_source_sha": upstream_sha,
                "upstream_source_dirty": upstream_dirty,
                "input_sha256": input_hashes[input_path.name],
                "binary_sha256": solver_hash,
                "command": shlex.join([
                    str(solver_path), f"--seed={args.solver_seed}", "<temporary-cnf>"
                ]),
                "host": platform.node(),
                "platform": platform.platform(),
                "runner_versions": runner_versions,
                "concurrency": 1,
                "recorded_at": datetime.datetime.now().astimezone().isoformat(),
                "solver": "CaDiCaL 1.9.5",
                "solver_seed": args.solver_seed,
                "method": args.method,
                "time_limit": args.time_limit,
                "search_strategy": "descending-linear",
            }
            rows.append(row)
            write_rows(args.output, rows)

    print(f"Complete result: {args.output} ({len(rows)} rows)")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Produce the prespecified R1.9 summaries from the official three-run CSVs."""

import argparse
import math
import statistics
from pathlib import Path

import pandas as pd
from scipy.stats import wilcoxon


REPETITIONS = {1, 2, 3}
MS_CAP_REPRESENTATIVES = {
    "c21_1_d1.col",
    "c21_2_d1.col",
    "c25_1_d3.col",
    "c55_1_d1.col",
}
BASELINES = ("POPH-S-B", "POP-S-B")
XA_LABEL = "Xa (fixed, x, Sym.)"


def as_bool(value):
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)) and not pd.isna(value):
        return bool(value)
    normalized = str(value).strip().lower()
    if normalized in {"true", "1", "yes"}:
        return True
    if normalized in {"false", "0", "no"}:
        return False
    raise ValueError(f"Cannot interpret Boolean value {value!r}")


def require_columns(frame, columns, label):
    missing = set(columns) - set(frame.columns)
    if missing:
        raise ValueError(f"{label}: missing columns {sorted(missing)}")


def validate_runtime_rows(frame, label):
    require_columns(
        frame,
        {
            "name", "run_id", "encoding_time", "total_solving_time", "time_used",
            "status", "timed_out", "optimality_proven", "source_dirty", "solver_seed",
            "time_limit", "concurrency",
        },
        label,
    )
    frame = frame.copy()
    frame["run_id"] = pd.to_numeric(frame["run_id"], errors="raise").astype(int)
    for column in ("encoding_time", "total_solving_time", "time_used"):
        frame[column] = pd.to_numeric(frame[column], errors="raise")
        if (~frame[column].map(math.isfinite)).any() or (frame[column] < 0).any():
            raise ValueError(f"{label}: {column} must be finite and nonnegative")
    for row in frame.itertuples(index=False):
        expected = float(row.encoding_time) + float(row.total_solving_time)
        if not math.isclose(float(row.time_used), expected, rel_tol=1e-9, abs_tol=1e-9):
            raise ValueError(
                f"{label}: time_used mismatch for {row.name}, run {row.run_id}: "
                f"{row.time_used} != {expected}"
            )
    if {str(value).strip() for value in frame["solver_seed"]} != {"0"}:
        raise ValueError(f"{label}: expected solver seed 0")
    if {float(value) for value in frame["time_limit"]} != {3600.0}:
        raise ValueError(f"{label}: expected a 3600-second limit")
    if {int(value) for value in frame["concurrency"]} != {1}:
        raise ValueError(f"{label}: expected concurrency one")
    if any(as_bool(value) for value in frame["source_dirty"]):
        raise ValueError(f"{label}: result was produced from a dirty source tree")
    frame["timed_out"] = frame["timed_out"].map(as_bool)
    frame["optimality_proven"] = frame["optimality_proven"].map(as_bool)
    frame["complete"] = (~frame["timed_out"]) & frame["optimality_proven"]
    return frame


def validate_three_runs(frame, label, expected_instances=53):
    duplicates = frame.duplicated(["name", "run_id"])
    if duplicates.any():
        raise ValueError(f"{label}: duplicate (name, run_id) rows")
    grouped = frame.groupby("name")["run_id"].apply(set)
    if len(grouped) != expected_instances:
        raise ValueError(f"{label}: expected {expected_instances} instances, found {len(grouped)}")
    invalid = grouped[grouped.map(lambda values: values != REPETITIONS)]
    if not invalid.empty:
        raise ValueError(f"{label}: every instance must have run_id 1, 2, and 3")


def statistical_units(xa):
    geom = {name for name in xa["name"] if str(name).upper().startswith("GEOM")}
    if len(geom) != 33:
        raise ValueError(f"Xa: expected 33 GEOM instances, found {len(geom)}")
    units = geom | MS_CAP_REPRESENTATIVES
    if len(units) != 37 or not units.issubset(set(xa["name"])):
        raise ValueError("Xa: the four prespecified MS-CAP representatives are incomplete")
    return sorted(units)


def prepare_inputs(xa_csv, pop_csv):
    xa = validate_runtime_rows(pd.read_csv(xa_csv), "Xa")
    require_columns(
        xa,
        {"method", "width", "incremental", "incremental_variable", "symmetry_breaking"},
        "Xa",
    )
    if {str(value) for value in xa["method"]} != {"Xa(cache)"}:
        raise ValueError("Xa: expected method Xa(cache)")
    if {str(value) for value in xa["width"]} != {"fixed"}:
        raise ValueError("Xa: expected fixed width")
    if not all(as_bool(value) for value in xa["incremental"]):
        raise ValueError("Xa: expected incremental solving")
    if {str(value) for value in xa["incremental_variable"]} != {"x"}:
        raise ValueError("Xa: expected incremental variable x")
    if not all(as_bool(value) for value in xa["symmetry_breaking"]):
        raise ValueError("Xa: expected symmetry breaking")
    validate_three_runs(xa, "Xa")
    xa["analysis_method"] = XA_LABEL

    pop = validate_runtime_rows(pd.read_csv(pop_csv), "POP baselines")
    require_columns(pop, {"method", "upstream_base_sha"}, "POP baselines")
    if set(pop["method"]) != set(BASELINES):
        raise ValueError(f"POP baselines: expected methods {BASELINES}")
    if set(pop["upstream_base_sha"].astype(str)) != {
        "8f19dbff4135e6cff9e4b147ebe8462603d5fe03"
    }:
        raise ValueError("POP baselines: unexpected upstream source commit")
    for method in BASELINES:
        validate_three_runs(pop[pop["method"] == method], method)
    pop["analysis_method"] = pop["method"]

    units = statistical_units(xa)
    if not set(units).issubset(set(pop["name"])):
        raise ValueError("POP baselines: missing one or more of the 37 statistical units")
    combined = pd.concat([xa, pop], ignore_index=True, sort=False)
    return combined, units


def instance_summary(frame, units):
    selected = frame[frame["name"].isin(units)].copy()
    summary = selected.groupby(["analysis_method", "name"], as_index=False).agg(
        repetitions=("run_id", "count"),
        mean_runtime=("time_used", "mean"),
        sample_sd=("time_used", "std"),
        completed_runs=("complete", "sum"),
    )
    summary["all_three_completed"] = summary["completed_runs"] == 3
    return summary


def holm_adjust(p_values):
    count = len(p_values)
    order = sorted(range(count), key=lambda index: p_values[index])
    adjusted = [0.0] * count
    running_max = 0.0
    for rank, index in enumerate(order):
        candidate = min(1.0, (count - rank) * p_values[index])
        running_max = max(running_max, candidate)
        adjusted[index] = running_max
    return adjusted


def confirmatory_results(summary):
    xa = summary[summary["analysis_method"] == XA_LABEL].set_index("name")
    rows = []
    raw_p_values = []
    for baseline in BASELINES:
        base = summary[summary["analysis_method"] == baseline].set_index("name")
        joined = xa[["mean_runtime", "all_three_completed"]].join(
            base[["mean_runtime", "all_three_completed"]],
            lsuffix="_xa",
            rsuffix="_baseline",
            how="inner",
        )
        paired = joined[
            joined["all_three_completed_xa"] & joined["all_three_completed_baseline"]
        ]
        if paired.empty:
            raise ValueError(f"{baseline}: no complete paired instances")
        differences = paired["mean_runtime_xa"] - paired["mean_runtime_baseline"]
        if (differences == 0).all():
            statistic, raw_p = 0.0, 1.0
        else:
            result = wilcoxon(differences, alternative="two-sided", zero_method="wilcox", method="auto")
            statistic, raw_p = float(result.statistic), float(result.pvalue)
        ratios = paired["mean_runtime_xa"] / paired["mean_runtime_baseline"]
        raw_p_values.append(raw_p)
        rows.append({
            "comparison": f"{XA_LABEL} vs {baseline}",
            "paired_n": len(paired),
            "wilcoxon_statistic": statistic,
            "raw_p_value": raw_p,
            "median_xa_over_baseline_runtime_ratio": statistics.median(ratios),
            "xa_complete_units": int(xa["all_three_completed"].sum()),
            "baseline_complete_units": int(base["all_three_completed"].sum()),
        })
    adjusted = holm_adjust(raw_p_values)
    for row, value in zip(rows, adjusted):
        row["holm_adjusted_p_value"] = value
    return pd.DataFrame(rows)


def benchmark_totals(frame, units):
    selected = frame[frame["name"].isin(units)].copy()
    by_run = selected.groupby(["analysis_method", "run_id"], as_index=False).agg(
        total_runtime=("time_used", "sum"),
        completed_units=("complete", "sum"),
        statistical_units=("name", "nunique"),
    )
    summary = by_run.groupby("analysis_method", as_index=False).agg(
        mean_total_runtime=("total_runtime", "mean"),
        sample_sd_total_runtime=("total_runtime", "std"),
        minimum_completed_units=("completed_units", "min"),
    )
    return by_run, summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xa-csv", type=Path, required=True)
    parser.add_argument("--pop-csv", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    frame, units = prepare_inputs(args.xa_csv, args.pop_csv)
    summary = instance_summary(frame, units)
    confirmatory = confirmatory_results(summary)
    totals_by_run, total_summary = benchmark_totals(frame, units)
    geom120b = summary[summary["name"].str.lower() == "geom120b.col"]

    args.output_dir.mkdir(parents=True, exist_ok=True)
    summary.to_csv(args.output_dir / "r1_9_instance_summary.csv", index=False)
    confirmatory.to_csv(args.output_dir / "r1_9_confirmatory.csv", index=False)
    totals_by_run.to_csv(args.output_dir / "r1_9_totals_by_run.csv", index=False)
    total_summary.to_csv(args.output_dir / "r1_9_total_summary.csv", index=False)
    geom120b.to_csv(args.output_dir / "r1_9_geom120b.csv", index=False)
    print(f"R1.9 analysis written to {args.output_dir} for {len(units)} statistical units")


if __name__ == "__main__":
    main()

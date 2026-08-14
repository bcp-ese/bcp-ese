import math
import sys
import tempfile
import unittest
from pathlib import Path

import pandas as pd

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))

import analyze_r1_9


class R19AnalysisTest(unittest.TestCase):
    def test_holm_adjustment_preserves_original_order(self):
        adjusted = analyze_r1_9.holm_adjust([0.04, 0.01])
        self.assertEqual(adjusted, [0.04, 0.02])

    def test_instance_summary_uses_sample_standard_deviation(self):
        frame = pd.DataFrame({
            "analysis_method": [analyze_r1_9.XA_LABEL] * 3,
            "name": ["GEOM20.col"] * 3,
            "run_id": [1, 2, 3],
            "time_used": [1.0, 2.0, 3.0],
            "complete": [True, True, True],
        })
        summary = analyze_r1_9.instance_summary(frame, ["GEOM20.col"])
        self.assertEqual(summary.loc[0, "repetitions"], 3)
        self.assertEqual(summary.loc[0, "mean_runtime"], 2.0)
        self.assertTrue(math.isclose(summary.loc[0, "sample_sd"], 1.0))
        self.assertTrue(summary.loc[0, "all_three_completed"])

    def test_runtime_validation_rejects_inconsistent_total(self):
        frame = pd.DataFrame({
            "name": ["GEOM20.col"],
            "run_id": [1],
            "encoding_time": [1.0],
            "total_solving_time": [2.0],
            "time_used": [4.0],
            "status": ["OPTIMAL"],
            "timed_out": [False],
            "optimality_proven": [True],
            "source_dirty": [False],
            "solver_seed": [0],
            "time_limit": [3600],
            "concurrency": [1],
        })
        with self.assertRaisesRegex(ValueError, "time_used mismatch"):
            analyze_r1_9.validate_runtime_rows(frame, "synthetic")

    def test_official_shape_produces_two_37_pair_comparisons(self):
        geom = [f"GEOM{index}.col" for index in range(1, 34)]
        representatives = sorted(analyze_r1_9.MS_CAP_REPRESENTATIVES)
        aliases = [f"alias_{index}.col" for index in range(1, 17)]
        names = geom + representatives + aliases

        common = {
            "status": "OPTIMAL",
            "timed_out": False,
            "optimality_proven": True,
            "source_dirty": False,
            "solver_seed": 0,
            "time_limit": 3600,
            "concurrency": 1,
        }
        xa_rows = []
        pop_rows = []
        for name_index, name in enumerate(names, start=1):
            for run_id in analyze_r1_9.REPETITIONS:
                encoding = 0.1
                xa_solving = name_index + run_id / 100
                xa_rows.append({
                    **common,
                    "name": name,
                    "run_id": run_id,
                    "encoding_time": encoding,
                    "total_solving_time": xa_solving,
                    "time_used": encoding + xa_solving,
                    "method": "Xa(cache)",
                    "width": "fixed",
                    "incremental": True,
                    "incremental_variable": "x",
                    "symmetry_breaking": True,
                })
                for method, multiplier in (("POPH-S-B", 1.1), ("POP-S-B", 1.2)):
                    solving = xa_solving * multiplier
                    pop_rows.append({
                        **common,
                        "name": name,
                        "run_id": run_id,
                        "encoding_time": encoding,
                        "total_solving_time": solving,
                        "time_used": encoding + solving,
                        "method": method,
                        "upstream_base_sha":
                            "8f19dbff4135e6cff9e4b147ebe8462603d5fe03",
                    })

        with tempfile.TemporaryDirectory() as temp_dir:
            xa_path = Path(temp_dir) / "xa.csv"
            pop_path = Path(temp_dir) / "pop.csv"
            pd.DataFrame(xa_rows).to_csv(xa_path, index=False)
            pd.DataFrame(pop_rows).to_csv(pop_path, index=False)
            frame, units = analyze_r1_9.prepare_inputs(xa_path, pop_path)

        summary = analyze_r1_9.instance_summary(frame, units)
        comparisons = analyze_r1_9.confirmatory_results(summary)
        self.assertEqual(len(units), 37)
        self.assertEqual(len(summary), 111)
        self.assertEqual(len(comparisons), 2)
        self.assertEqual(set(comparisons["paired_n"]), {37})


if __name__ == "__main__":
    unittest.main()

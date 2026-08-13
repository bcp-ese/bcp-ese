import sys
import tempfile
import unittest
from pathlib import Path

import pandas as pd

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))

import benchmark


class BenchmarkExportTest(unittest.TestCase):
    def test_missing_zero_counters_are_normalized(self):
        stats = benchmark.normalize_cadical_counters({
            'conflicts': 4,
            'propagations': 100,
        })

        self.assertEqual(stats['conflicts'], 4)
        self.assertEqual(stats['propagations'], 100)
        self.assertEqual(stats['decisions'], 0)
        self.assertEqual(stats['learned'], 0)
        self.assertEqual(stats['learned_lits'], 0)

    def test_append_preserves_unexpected_solver_counters(self):
        frame = pd.DataFrame(columns=benchmark.RESULT_COLUMNS)
        row = benchmark.normalize_cadical_counters({
            'name': 'tiny.col',
            'conflicts': 7,
            'decisions': 11,
            'propagations': 101,
            'learned': 6,
            'learned_lits': 23,
            'restarts': 1,
            'reduced': 0,
            'future_cadical_counter': 13,
        })

        frame = benchmark.append_result(frame, row)

        self.assertIn('future_cadical_counter', frame.columns)
        self.assertEqual(frame.loc[0, 'conflicts'], 7)
        self.assertEqual(frame.loc[0, 'future_cadical_counter'], 13)

        with tempfile.TemporaryDirectory() as temp_dir:
            csv_path = Path(temp_dir) / 'result.csv'
            frame.to_csv(csv_path, index=False)
            reloaded = pd.read_csv(csv_path)

        self.assertIn('future_cadical_counter', reloaded.columns)
        self.assertEqual(reloaded.loc[0, 'conflicts'], 7)
        self.assertEqual(reloaded.loc[0, 'future_cadical_counter'], 13)


if __name__ == '__main__':
    unittest.main()

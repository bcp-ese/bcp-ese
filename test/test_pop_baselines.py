import sys
import tempfile
import unittest
from pathlib import Path

import networkx as nx

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))

import benchmark_pop_baselines as pop


class PopBaselineAdapterTest(unittest.TestCase):
    def test_read_projection_ignores_demands_and_self_loops(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "tiny.col"
            path.write_text(
                "p band 3 3\n"
                "n 1 4\n"
                "e 1 1 7\n"
                "e 1 2 2\n"
                "e 2 3 3\n",
                encoding="utf-8",
            )
            graph = pop.read_bcp_graph(path)

        self.assertEqual(graph.number_of_nodes(), 3)
        self.assertEqual(graph.number_of_edges(), 2)
        self.assertEqual(graph[0][1]["weight"], 2)
        self.assertEqual(graph[1][2]["weight"], 3)

    def test_greedy_upper_bound_uses_positive_domain_size(self):
        graph = nx.Graph()
        graph.add_nodes_from(range(2))
        graph.add_edge(0, 1, weight=3)
        self.assertEqual(pop.greedy_upper_bound(graph), 4)

    def test_parse_dimacs_result_uses_literal_indices(self):
        status, values = pop.parse_dimacs_result(
            "s SATISFIABLE\nv 1 -2 3 0\n"
        )
        self.assertEqual(status, "SATISFIABLE")
        self.assertEqual(values, [0, 1, 0, 1])

    def test_parse_unsatisfiable(self):
        status, values = pop.parse_dimacs_result("s UNSATISFIABLE\n")
        self.assertEqual(status, "UNSATISFIABLE")
        self.assertIsNone(values)

    def test_resume_rejects_different_protocol(self):
        expected = {"method": "POP-S-B", "time_limit": 3600.0}
        row = {column: "" for column in pop.RESULT_COLUMNS}
        row.update({
            "name": "tiny.col", "run_id": "1", "input_sha256": "abc",
            "method": "POPH-S-B", "time_limit": "3600.0",
            "encoding_time": "1.0", "total_solving_time": "2.0",
            "time_used": "3.0",
        })
        with self.assertRaisesRegex(RuntimeError, "method"):
            pop.validate_existing_rows(
                [row], expected, {"tiny.col": "abc"}, repetitions=3
            )


if __name__ == "__main__":
    unittest.main()

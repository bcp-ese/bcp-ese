#include "test_common.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <vector>

namespace
{
    struct Case
    {
        std::string name;
        int nodes;
        std::vector<std::tuple<int, int, int>> edges;
    };

    bool extend_coloring(const BCPSolver::Graph& graph, const int vertex, const int span,
                         std::vector<int>& colors)
    {
        if (vertex == graph.get_number_of_nodes())
        {
            return true;
        }

        for (int color = 1; color <= span; ++color)
        {
            bool valid = true;
            for (const auto& [u, v, weight] : graph.get_edges())
            {
                if (u == vertex && v < vertex && std::abs(color - colors[v]) < weight)
                {
                    valid = false;
                    break;
                }
                if (v == vertex && u < vertex && std::abs(color - colors[u]) < weight)
                {
                    valid = false;
                    break;
                }
            }
            if (valid)
            {
                colors[vertex] = color;
                if (extend_coloring(graph, vertex + 1, span, colors))
                {
                    return true;
                }
            }
        }
        colors[vertex] = 0;
        return false;
    }

    int brute_force_optimum(const BCPSolver::Graph& graph)
    {
        for (int span = 1; span <= 12; ++span)
        {
            std::vector<int> colors(graph.get_number_of_nodes());
            if (extend_coloring(graph, 0, span, colors))
            {
                return span;
            }
        }
        return -1;
    }

    BCPSolver::Graph make_graph(const Case& test_case)
    {
        BCPSolver::Graph graph(test_case.nodes);
        for (const auto& [u, v, weight] : test_case.edges)
        {
            graph.add_edge(u, v, weight);
        }
        return graph;
    }

    struct Encoding
    {
        BCPSolver::SolvingMethod method;
        const char* width;
        const char* variable;
    };

    std::filesystem::path write_test_graph(const std::string& name, const std::string& contents)
    {
        const auto path = std::filesystem::path(testing::TempDir()) / name;
        std::ofstream output(path);
        output << contents;
        output.close();
        return path;
    }
}

TEST(SmallGraphEncodingTest, AllEncodingsAgreeWithBruteForce)
{
    std::vector<Case> cases = {
        {"edgeless-3", 3, {}},
        {"K2-d1", 2, {{0, 1, 1}}},
        {"K2-d3", 2, {{0, 1, 3}}},
        {"edge-plus-isolated", 3, {{0, 1, 2}}},
        {"mixed-weight-path", 4, {{0, 1, 2}, {1, 2, 3}, {2, 3, 1}}},
        {"mixed-weight-cycle", 4, {{0, 1, 1}, {1, 2, 2}, {2, 3, 3}, {3, 0, 2}}},
        {"triangle-d1", 3, {{0, 1, 1}, {0, 2, 1}, {1, 2, 1}}},
        {"weighted-triangle", 3, {{0, 1, 2}, {0, 2, 3}, {1, 2, 1}}},
        {"K4-d1", 4, {{0, 1, 1}, {0, 2, 1}, {0, 3, 1}, {1, 2, 1}, {1, 3, 1}, {2, 3, 1}}},
        {"odd-midpoint", 3, {{0, 1, 2}, {0, 2, 2}, {1, 2, 4}}},
    };
    std::mt19937 random(20260812);
    std::bernoulli_distribution has_edge(0.55);
    std::uniform_int_distribution<int> edge_weight(1, 3);
    for (int case_index = 0; case_index < 16; ++case_index)
    {
        Case test_case{"random-" + std::to_string(case_index), 2 + case_index % 3, {}};
        for (int u = 0; u < test_case.nodes; ++u)
        {
            for (int v = u + 1; v < test_case.nodes; ++v)
            {
                if (has_edge(random))
                {
                    test_case.edges.emplace_back(u, v, edge_weight(random));
                }
            }
        }
        cases.push_back(std::move(test_case));
    }
    const std::vector<Encoding> encodings = {
        {BCPSolver::OneVariableGreater, "", "y"},
        {BCPSolver::OneVariableLess, "", "y"},
        {BCPSolver::TwoVariablesGreater, "", "x"},
        {BCPSolver::TwoVariablesGreater, "", "y"},
        {BCPSolver::TwoVariablesGreater, "", "both"},
        {BCPSolver::TwoVariablesLess, "", "x"},
        {BCPSolver::TwoVariablesLess, "", "y"},
        {BCPSolver::TwoVariablesLess, "", "both"},
        {BCPSolver::StaircaseWithAuxiliaryVarsNoCache, "vary", "x"},
        {BCPSolver::StaircaseWithAuxiliaryVarsNoCache, "fixed", "x"},
        {BCPSolver::StaircaseWithAuxiliaryVarsWithCache, "vary", "x"},
        {BCPSolver::StaircaseWithAuxiliaryVarsWithCache, "fixed", "x"},
        {BCPSolver::StaircaseWithoutAuxiliaryVars, "vary", "x"},
        {BCPSolver::StaircaseWithoutAuxiliaryVars, "fixed", "x"},
    };

    for (const auto& test_case : cases)
    {
        auto graph = make_graph(test_case);
        const int expected_span = brute_force_optimum(graph);
        ASSERT_GT(expected_span, 0) << test_case.name;

        for (const auto& encoding : encodings)
        {
            for (const bool symmetry : {false, true})
            {
                for (const bool incremental : {false, true})
                {
                    SCOPED_TRACE(std::string(test_case.name) + " method=" +
                                 std::to_string(encoding.method) + " width=" + encoding.width +
                                 " variable=" + encoding.variable + " symmetry=" +
                                 std::to_string(symmetry) + " incremental=" + std::to_string(incremental));
                    const auto solver = BCPSolver::test::make_solver(
                        encoding.method, &graph, expected_span + 1, symmetry, false, encoding.width);
                    ASSERT_NE(solver, nullptr);
                    const auto result = solver->solve(BCPSolver::NO_TIME_LIMIT, true, incremental,
                                                      incremental ? encoding.variable : "");
                    EXPECT_EQ(result, BCPSolver::OPTIMAL);
                    EXPECT_EQ(solver->get_span(), expected_span);
                }
            }
        }
    }
}

TEST(SmallGraphEncodingTest, EmptyGraphHasZeroSpan)
{
    BCPSolver::Graph graph(0);
    const std::vector<Encoding> encodings = {
        {BCPSolver::OneVariableGreater, "", "y"},
        {BCPSolver::OneVariableLess, "", "y"},
        {BCPSolver::TwoVariablesGreater, "", "both"},
        {BCPSolver::TwoVariablesLess, "", "both"},
        {BCPSolver::StaircaseWithAuxiliaryVarsNoCache, "vary", "x"},
        {BCPSolver::StaircaseWithAuxiliaryVarsWithCache, "fixed", "x"},
        {BCPSolver::StaircaseWithoutAuxiliaryVars, "vary", "x"},
    };

    for (const auto& encoding : encodings)
    {
        SCOPED_TRACE(std::string("method=") + std::to_string(encoding.method) +
                     " width=" + encoding.width);
        const auto solver = BCPSolver::test::make_solver(
            encoding.method, &graph, -1, true, false, encoding.width);
        ASSERT_NE(solver, nullptr);
        EXPECT_EQ(solver->solve(BCPSolver::NO_TIME_LIMIT, true, false, ""), BCPSolver::OPTIMAL);
        EXPECT_EQ(solver->get_span(), 0);
    }
}

TEST(ParserTest, ProjectsMsCapByIgnoringDemandRecordsAndSelfLoops)
{
    const auto path = write_test_graph(
        "bcp-valid-ms-cap.col",
        "c minimal MS-CAP-style input\n"
        "p band 3 4\n"
        "n 1 2\n"
        "n 2 1\n"
        "n 3 4\n"
        "e 1 1 2\n"
        "e 1 2 3\n"
        "e 2 2 1\n"
        "e 3 3 4\n");

    const std::unique_ptr<BCPSolver::Graph> graph(BCPSolver::read_bcp_graph(path.string()));
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->get_number_of_nodes(), 3);
    EXPECT_EQ(graph->get_number_of_edges(), 1);
    EXPECT_EQ(graph->get_weight(0, 1), 3);
    std::filesystem::remove(path);
}

TEST(ParserTest, RejectsMalformedGraphInvariants)
{
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"bcp-edge-count-mismatch.col", "p band 2 2\ne 1 2 1\n"},
        {"bcp-edge-before-header.col", "e 1 2 1\np band 2 1\n"},
        {"bcp-invalid-header.col", "p edge 2 1\ne 1 2 1\n"},
        {"bcp-invalid-endpoint.col", "p band 2 1\ne 1 3 1\n"},
        {"bcp-invalid-weight.col", "p band 2 1\ne 1 2 0\n"},
        {"bcp-invalid-demand.col", "p band 2 0\nn 3 1\n"},
        {"bcp-duplicate-edge.col", "p band 2 2\ne 1 2 1\ne 2 1 1\n"},
        {"bcp-unknown-record.col", "p band 2 0\nz 1 2\n"},
    };

    for (const auto& [name, contents] : cases)
    {
        const auto path = write_test_graph(name, contents);
        SCOPED_TRACE(name);
        const std::unique_ptr<BCPSolver::Graph> graph(BCPSolver::read_bcp_graph(path.string()));
        EXPECT_EQ(graph, nullptr);
        std::filesystem::remove(path);
    }
}

TEST(SolverStatusTest, TimeLimitIsReportedSeparatelyFromOptimality)
{
    const auto graph = BCPSolver::test::load_graph("../dataset/GEOM20.col");
    ASSERT_NE(graph, nullptr);
    const auto solver = BCPSolver::test::make_solver(
        BCPSolver::OneVariableGreater, graph.get(), 100, false, false, "");
    ASSERT_NE(solver, nullptr);

    EXPECT_EQ(solver->solve(0.0, true, false, ""), BCPSolver::UNKNOWN);
    const auto stats = solver->get_statistics();
    EXPECT_EQ(stats.at("timed_out"), 1.0);
    EXPECT_EQ(stats.at("optimality_proven"), 0.0);
}

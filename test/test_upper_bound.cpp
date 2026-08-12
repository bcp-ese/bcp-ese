#include "test_common.h"

#include <random>

TEST(UpperBoundTest, ConvertsZeroBasedGreedyColorToPositiveSpan)
{
    BCPSolver::Graph graph(2);
    graph.add_edge(0, 1, 1);

    const auto solver = BCPSolver::test::make_solver(
        BCPSolver::OneVariableGreater,
        &graph,
        -1,
        false,
        false,
        "");

    ASSERT_NE(solver, nullptr);
    EXPECT_EQ(solver->solve(BCPSolver::NO_TIME_LIMIT, false, false), BCPSolver::SATISFIABLE);
    EXPECT_EQ(solver->get_span(), 2);
}

TEST(UpperBoundTest, GreedyBoundIsFeasibleOnDeterministicRandomGraphs)
{
    std::mt19937 random(20260812);
    std::bernoulli_distribution has_edge(0.5);
    std::uniform_int_distribution<int> edge_weight(1, 4);

    for (int case_index = 0; case_index < 64; ++case_index)
    {
        const int nodes = 2 + case_index % 5;
        BCPSolver::Graph graph(nodes);
        for (int u = 0; u < nodes; ++u)
        {
            for (int v = u + 1; v < nodes; ++v)
            {
                if (has_edge(random))
                {
                    graph.add_edge(u, v, edge_weight(random));
                }
            }
        }

        SCOPED_TRACE("case=" + std::to_string(case_index));
        const auto solver = BCPSolver::test::make_solver(
            BCPSolver::OneVariableGreater, &graph, -1, false, false, "");
        ASSERT_NE(solver, nullptr);
        EXPECT_EQ(solver->solve(BCPSolver::NO_TIME_LIMIT, false, false), BCPSolver::SATISFIABLE);
        EXPECT_GE(solver->get_span(), 1);
    }
}

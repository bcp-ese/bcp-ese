#include "test_common.h"

using BCPSolver::SolverStatus;
using BCPSolver::test::solve_expect;

TEST(OneVariableLessEncodingTest, GEOM20_NonOptimal_DummyUpperBound)
{
    for (const bool symm : {false, true})
    {
        constexpr int ub = 100;
        SCOPED_TRACE(symm ? "symmetry=on" : "symmetry=off");
        solve_expect(BCPSolver::OneVariableLess, "../dataset/GEOM20.col", ub, symm, false, "", false, false, "y",
                     SolverStatus::SATISFIABLE, ub);
    }
}

TEST(OneVariableLessEncodingTest, Optimal_NonIncremental_GEOM20_GEOM20a_GEOM20b)
{
    struct Case
    {
        const char* path;
        int expected_span;
    };
    constexpr Case cases[] = {
        {"../dataset/GEOM20.col", 21},
        {"../dataset/GEOM20a.col", 20},
        {"../dataset/GEOM20b.col", 13}
    };

    for (const auto& [path, expected_span] : cases)
    {
        for (const bool symm : {false, true})
        {
            SCOPED_TRACE(std::string(path) + " / " + (symm ? "symmetry=on" : "symmetry=off"));
            solve_expect(BCPSolver::OneVariableLess, path, -1, symm, false, "", true, false, "",
                         SolverStatus::OPTIMAL, expected_span);
        }
    }
}

TEST(OneVariableLessEncodingTest, Optimal_Incremental_GEOM20_GEOM20a_GEOM20b)
{
    struct Case
    {
        const char* path;
        int expected_span;
    };
    constexpr Case cases[] = {
        {"../dataset/GEOM20.col", 21},
        {"../dataset/GEOM20a.col", 20},
        {"../dataset/GEOM20b.col", 13}
    };
    for (const auto& [path, expected_span] : cases)
    {
        for (const bool symm : {false, true})
        {
            SCOPED_TRACE(std::string(path) + " / " + (symm ? "symmetry=on" : "symmetry=off"));
            solve_expect(BCPSolver::OneVariableLess, path, -1, symm, false, "", true, true, "y",
                         SolverStatus::OPTIMAL, expected_span);
        }
    }
}

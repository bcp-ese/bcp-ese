#include "test_common.h"

using BCPSolver::SolverStatus;
using BCPSolver::test::solve_expect;

TEST(TwoVariableLessEncodingTest, GEOM20_NonOptimal_DummyUpperBound_AllFlagCombos)
{
    for (const auto& solver : {SATSolver::SOLVER::CADICAL})
    {
        for (const bool symm : {false, true})
        {
            for (const bool heur : {false, true})
            {
                constexpr int ub = 100;
                SCOPED_TRACE(
                    std::string("symmetry=") + (symm ? "on" : "off") + " / heuristic=" + (heur ? "on" : "off"));
                solve_expect(BCPSolver::TwoVariablesLess, "../dataset/GEOM20.col", solver, ub, symm, heur, "", false,
                             false, "", SolverStatus::SATISFIABLE, ub);
            }
        }
    }
}

TEST(TwoVariableLessEncodingTest, Optimal_NonIncremental_GEOM20_GEOM20a_GEOM20b)
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

    for (const auto& solver : {SATSolver::SOLVER::CADICAL})
    {
        for (const auto& [path, expected_span] : cases)
        {
            for (const bool symm : {false, true})
            {
                for (const bool heur : {false, true})
                {
                    SCOPED_TRACE(std::string(path) + " / symmetry=" + (symm ? "on" : "off") + " / heuristic=" +
                        (heur ? "on" : "off"));
                    solve_expect(BCPSolver::TwoVariablesLess, path, solver, -1, symm, heur, "", true, false, "",
                                 SolverStatus::OPTIMAL, expected_span);
                }
            }
        }
    }
}

TEST(TwoVariableLessEncodingTest, Optimal_Incremental_GEOM20_GEOM20a_GEOM20b)
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

    for (const auto& solver : {SATSolver::SOLVER::CADICAL})
    {
        for (const auto& [path, expected_span] : cases)
        {
            for (const bool symm : {false, true})
            {
                for (const bool heur : {false, true})
                {
                    for (const std::string variable_for_incremental : {"x", "y", "both"})
                    {
                        SCOPED_TRACE(std::string(path) + " / symmetry=" + (symm ? "on" : "off") + " / heuristic=" +
                            (heur ? "on" : "off") + " / variable_for_incremental=" + variable_for_incremental);
                        solve_expect(BCPSolver::TwoVariablesLess, path, solver, -1, symm, heur, "", true, true,
                                     variable_for_incremental, SolverStatus::OPTIMAL, expected_span);
                    }
                }
            }
        }
    }
}

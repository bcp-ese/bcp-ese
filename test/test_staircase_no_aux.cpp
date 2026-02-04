#include "test_common.h"

using BCPSolver::SolverStatus;
using BCPSolver::test::solve_expect;

TEST(StaircaseWithoutAuxiliaryVarsEncodingTest, GEOM20_NonOptimal_DummyUpperBound)
{
    for (const auto& solver : {SATSolver::SOLVER::CADICAL})
    {
        for (const bool symm : {false, true})
        {
            for (const bool heur : {false, true})
            {
                for (const auto width : {"vary", "fixed"})
                {
                    constexpr int ub = 100;
                    SCOPED_TRACE(std::string(symm ? "symmetry=on" : "symmetry=off") + " / " +
                        (heur ? "heuristic=on" : "heuristic=off") + " / width=" + width);
                    solve_expect(BCPSolver::StaircaseWithoutAuxiliaryVars, "../dataset/GEOM20.col", solver, ub,
                                 symm, heur, width, false, false, "", SolverStatus::SATISFIABLE, ub);
                }
            }
        }
    }
}

TEST(StaircaseWithoutAuxiliaryVarsEncodingTest, Optimal_NonIncremental_GEOM20_GEOM20a_GEOM20b)
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
                    for (const auto width : {"vary", "fixed"})
                    {
                        SCOPED_TRACE(std::string(path) + " / symmetry=" + (symm ? "on" : "off") + " / heuristic=" +
                            (heur ? "on" : "off") + " / width=" + width);
                        solve_expect(BCPSolver::StaircaseWithoutAuxiliaryVars, path, solver, -1, symm, heur, width,
                                     true, false, "", SolverStatus::OPTIMAL, expected_span);
                    }
                }
            }
        }
    }
}

TEST(StaircaseWithoutAuxiliaryVarsEncodingTest, Optimal_Incremental_GEOM20_GEOM20a_GEOM20b)
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
                    for (const auto width : {"vary", "fixed"})
                    {
                        SCOPED_TRACE(std::string(path) + " / symmetry=" + (symm ? "on" : "off") + " / heuristic=" +
                            (heur ? "on" : "off") + " / width=" + width);
                        solve_expect(BCPSolver::StaircaseWithoutAuxiliaryVars, path, solver, -1, symm, heur, width,
                                     true, true,
                                     "x", SolverStatus::OPTIMAL, expected_span);
                    }
                }
            }
        }
    }
}

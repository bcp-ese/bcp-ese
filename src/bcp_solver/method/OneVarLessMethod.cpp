//
// Created by homulily85 on 12/30/25.
//

#include "OneVarLessMethod.h"

void BCPSolver::OneVarLessMethod::symmetry_breaking()
{
    if (graph->get_number_of_nodes() == 0)
    {
        return;
    }
    const int midpoint = (span + 1) / 2;
    sat_solver->add_clause(y.at({graph->get_highest_degree_vertex(), midpoint}));
}

void BCPSolver::OneVarLessMethod::first_constraint()
{
    for (int i = 0; i < graph->get_number_of_nodes(); i++)
    {
        sat_solver->add_clause(y[{i, span}]);
    }
}

void BCPSolver::OneVarLessMethod::second_constraint()
{
    for (int i = 0; i < graph->get_number_of_nodes(); i++)
    {
        for (int c = 1; c < span; c++)
        {
            sat_solver->add_clause(-y[{i, c}], y[{i, c + 1}]);
        }
    }
}

void BCPSolver::OneVarLessMethod::third_constraint()
{
    for (const auto& edge : graph->get_edges())
    {
        auto [u, v, weight] = edge;

        for (int c = 1; c <= span; c++)
        {
            if (c - weight < 1 && c + weight - 1 > span)
            {
                if (c == 1)
                {
                    sat_solver->add_clause(-y.at({u, c}));
                }
                else
                {
                    sat_solver->add_clause(-y.at({u, c}), y.at({u, c - 1}));
                }
            }
            else if (c - weight < 1)
            {
                if (c == 1)
                {
                    sat_solver->add_clause(-y[{u, c}], -y[{v, c + weight - 1}]);
                }
                else
                {
                    sat_solver->add_clause(-y[{u, c}], y[{u, c - 1}], -y[{v, c + weight - 1}]);
                }
            }
            else if (c + weight - 1 > span)
            {
                if (c == 1)
                {
                    sat_solver->add_clause(-y[{u, c}]);
                }
                else
                {
                    sat_solver->add_clause(-y[{u, c}], y[{u, c - 1}], y[{v, c - weight}]);
                }
            }
            else
            {
                sat_solver->add_clause(-y[{u, c}], y[{u, c - 1}], -y[{v, c + weight - 1}], y[{v, c - weight}]);
            }
        }
    }
}

void BCPSolver::OneVarLessMethod::encode()
{
    const auto start_time = std::chrono::high_resolution_clock::now();

    create_variable();

    if (use_symmetry_breaking)
    {
        symmetry_breaking();
    }

    first_constraint();
    second_constraint();
    third_constraint();

    encoding_time += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start_time).count();
}

void BCPSolver::OneVarLessMethod::create_variable()
{
    y.clear();

    for (int i = 0; i < graph->get_number_of_nodes(); i++)
    {
        for (int c = 1; c < span + 1; c++)
        {
            y.insert(std::pair<std::pair<int, int>, int>({i, c}, sat_solver->create_new_variable()));
        }
    }
}

std::vector<int> BCPSolver::OneVarLessMethod::create_bound_tightening_literals(
    const std::string& variable_for_incremental)
{
    if (variable_for_incremental == "y")
    {
        std::vector<int> literals(graph->get_number_of_nodes());

        for (int i = 0; i < graph->get_number_of_nodes(); i++)
        {
            literals[i] = y.at({i, span - 1});
        }
        return literals;
    }
    else
    {
        throw std::runtime_error("Invalid variable for incremental in OneVarLessMethod.");
    }
}

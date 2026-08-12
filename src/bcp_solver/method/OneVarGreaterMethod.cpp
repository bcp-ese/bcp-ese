//
// Created by homulily85 on 12/30/25.
//

#include "OneVarGreaterMethod.h"

void BCPSolver::OneVarGreaterMethod::first_constraint()
{
    for (int i = 0; i < graph->get_number_of_nodes(); i++)
    {
        sat_solver->add_clause(y[{i, 1}]);
    }
}

void BCPSolver::OneVarGreaterMethod::second_constraint()
{
    for (int i = 0; i < graph->get_number_of_nodes(); i++)
    {
        for (int c = 2; c < span + 1; c++)
        {
            sat_solver->add_clause(-y[{i, c}], y[{i, c - 1}]);
        }
    }
}

void BCPSolver::OneVarGreaterMethod::third_constraint()
{
    for (const auto& edge : graph->get_edges())
    {
        auto [u, v, weight] = edge;
        for (int c = 1; c <= span; c++)
        {
            if (c - weight < 0 && c + weight > span)
            {
                if (c == span)
                {
                    sat_solver->add_clause(-y[{u, c}]);
                }
                else
                {
                    sat_solver->add_clause(-y[{u, c}], y[{u, c + 1}]);
                }
            }
            else if (c - weight < 0)
            {
                sat_solver->add_clause(-y[{u, c}], y[{u, c + 1}], y[{v, c + weight}]);
            }
            else if (c + weight > span)
            {
                if (c == span)
                {
                    sat_solver->add_clause(-y[{u, c}], -y[{v, c - weight + 1}]);
                }
                else
                {
                    sat_solver->add_clause(-y[{u, c}], y[{u, c + 1}], -y[{v, c - weight + 1}]);
                }
            }
            else
            {
                sat_solver->add_clause(-y[{u, c}], y[{u, c + 1}], y[{v, c + weight}], -y[{v, c - weight + 1}]);
            }
        }
    }
}

void BCPSolver::OneVarGreaterMethod::symmetry_breaking()
{
    if (graph->get_number_of_nodes() == 0)
    {
        return;
    }
    const int midpoint = (span + 1) / 2;
    if (midpoint < span)
    {
        sat_solver->add_clause(-y.at({graph->get_highest_degree_vertex(), midpoint + 1}));
    }
}

void BCPSolver::OneVarGreaterMethod::encode()
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

void BCPSolver::OneVarGreaterMethod::create_variable()
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

std::vector<int> BCPSolver::OneVarGreaterMethod::create_bound_tightening_literals(
    const std::string& variable_for_incremental)
{
    if (variable_for_incremental == "y")
    {
        std::vector<int> literals(graph->get_number_of_nodes());

        for (int i = 0; i < graph->get_number_of_nodes(); i++)
        {
            literals[i] = -y.at({i, span});
        }
        return literals;
    }
    else
    {
        throw std::runtime_error("Invalid variable for incremental in OneVarGreaterMethod.");
    }
}

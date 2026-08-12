//
// Created by homulily85 on 12/30/25.
//

#include "TwoVarsGreaterMethod.h"

void BCPSolver::TwoVarsGreaterMethod::symmetry_breaking()
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

void BCPSolver::TwoVarsGreaterMethod::first_constraint()
{
    for (int i = 0; i < graph->get_number_of_nodes(); i++)
    {
        for (int c = 1; c < span + 1; c++)
        {
            if (c == span)
            {
                sat_solver->add_clause(-x[{i, c}], y[{i, c}]);
                sat_solver->add_clause(x[{i, c}], -y[{i, c}]);
            }
            else
            {
                sat_solver->add_clause(-x[{i, c}], y[{i, c}]);
                sat_solver->add_clause(-x[{i, c}], -y[{i, c + 1}]);
                sat_solver->add_clause(x[{i, c}], -y[{i, c}], y[{i, c + 1}]);
            }
        }
    }
}

void BCPSolver::TwoVarsGreaterMethod::second_constraint()
{
    for (int i = 0; i < graph->get_number_of_nodes(); i++)
    {
        sat_solver->add_clause(y[{i, 1}]);
    }
}

void BCPSolver::TwoVarsGreaterMethod::third_constraint()
{
    for (int i = 0; i < graph->get_number_of_nodes(); i++)
    {
        for (int c = 2; c < span + 1; c++)
        {
            sat_solver->add_clause(-y[{i, c}], y[{i, c - 1}]);
        }
    }
}

void BCPSolver::TwoVarsGreaterMethod::fourth_constraint()
{
    for (const auto& edge : graph->get_edges())
    {
        auto [u, v, weight] = edge;
        if (use_heuristic)
        {
            if (weight == 1)
            {
                for (int c = 1; c <= span; c++)
                {
                    if (c - 1 < 0 && c + 1 > span)
                    {
                        sat_solver->add_clause(-x[{u, c}]);
                    }
                    else
                    {
                        sat_solver->add_clause(-x[{u, c}], -x[{v, c}]);
                    }
                }
                continue;
            }
        }

        for (int c = 1; c <= span; c++)
        {
            if (c - weight < 0 && c + weight > span)
            {
                sat_solver->add_clause(-x[{u, c}]);
            }
            else if (c - weight < 0)
            {
                sat_solver->add_clause(-x[{u, c}], y[{v, c + weight}]);
            }
            else if (c + weight > span)
            {
                sat_solver->add_clause(-x[{u, c}], -y[{v, c - weight + 1}]);
            }
            else
            {
                sat_solver->add_clause(-x[{u, c}], y[{v, c + weight}], -y[{v, c - weight + 1}]);
            }
        }
    }
}

void BCPSolver::TwoVarsGreaterMethod::encode()
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
    fourth_constraint();

    encoding_time += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start_time).count();
}

void BCPSolver::TwoVarsGreaterMethod::create_variable()
{
    x.clear();
    y.clear();

    for (int i = 0; i < graph->get_number_of_nodes(); i++)
    {
        for (int c = 1; c < span + 1; c++)
        {
            x.insert(std::pair<std::pair<int, int>, int>({i, c}, sat_solver->create_new_variable()));
            y.insert(std::pair<std::pair<int, int>, int>({i, c}, sat_solver->create_new_variable()));
        }
    }
}

std::vector<int> BCPSolver::TwoVarsGreaterMethod::create_bound_tightening_literals(
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
    else if (variable_for_incremental == "x")
    {
        std::vector<int> literals(graph->get_number_of_nodes());

        for (int i = 0; i < graph->get_number_of_nodes(); i++)
        {
            literals[i] = -x.at({i, span});
        }
        return literals;
    }
    else if (variable_for_incremental == "both")
    {
        std::vector<int> literals;
        literals.reserve(2 * graph->get_number_of_nodes());

        for (int i = 0; i < graph->get_number_of_nodes(); i++)
        {
            literals.push_back(-x.at({i, span}));
            literals.push_back(-y.at({i, span}));
        }
        return literals;
    }
    else
    {
        throw std::runtime_error("Invalid variable for incremental in TwoVarsGreaterMethod.");
    }
}

//
// Created by homulily85 on 11/29/25.
//

#include "bcp_solver.h"

#include <chrono>
#include <queue>
#include <ranges>
#include <utility>

#include "method/OneVarGreaterMethod.h"
#include "method/OneVarLessMethod.h"
#include "method/StaircaseWithAuxiliaryVarsMethod.h"
#include "method/StaircaseWithoutAuxiliaryVarsMethod.h"
#include "method/TwoVarsGreaterMethod.h"
#include "method/TwoVarsLessMethod.h"
#include "sat_solver/Cadical.h"

void BCPSolver::BCPSolver::calculate_upper_bound()
{
    const int n = graph->get_number_of_nodes();
    if (n == 0)
    {
        upper_bound = 0;
        return;
    }

    std::vector colors(n, -1);

    // For BCP, q_v = 1 for every vertex in the source greedy heuristic. The
    // priority below is therefore only a deterministic tie-break by vertex ID;
    // it is not a DSatur score.
    std::priority_queue<int, std::vector<int>, std::greater<>> pending_vertices;

    int max_color = 0;

    std::vector<std::vector<std::pair<int, int>>> adj(n);
    for (const auto edges = graph->get_edges(); const auto& edge : edges)
    {
        int u = std::get<0>(edge);
        int v = std::get<1>(edge);
        int w = std::get<2>(edge);

        if (u >= 0 && u < n && v >= 0 && v < n)
        {
            adj[u].emplace_back(v, w);
            adj[v].emplace_back(u, w);
        }
    }

    auto has_uncolored = [&]() -> bool
    {
        return std::any_of(colors.begin(), colors.end(), [](const int c) { return c == -1; });
    };

    auto get_start_node = [&]() -> int
    {
        int best_node = -1;
        int max_deg = -1;
        for (int i = 0; i < n; ++i)
        {
            if (colors[i] == -1)
            {
                if (const int deg = static_cast<int>(adj[i].size()); deg > max_deg)
                {
                    max_deg = deg;
                    best_node = i;
                }
            }
        }
        return best_node;
    };

    while (has_uncolored())
    {
        if (int start_node = get_start_node(); start_node != -1)
        {
            pending_vertices.push(start_node);
        }

        while (!pending_vertices.empty())
        {
            const int v = pending_vertices.top();
            pending_vertices.pop();

            if (colors[v] > -1)
            {
                continue;
            }

            colors[v] = 0;

            std::vector<std::pair<int, int>> intervals;
            for (const auto& neighbor : adj[v])
            {
                int w = neighbor.first;
                int weight = neighbor.second;

                if (colors[w] > -1)
                {
                    intervals.emplace_back(colors[w] - weight, colors[w] + weight);
                }
            }

            std::sort(intervals.begin(), intervals.end());

            size_t invIndx = 0;
            while (invIndx < intervals.size() && colors[v] > intervals[invIndx].first)
            {
                colors[v] = std::max(colors[v], intervals[invIndx].second);
                invIndx++;
            }

            max_color = std::max(max_color, colors[v]);

            for (const auto& key : adj[v] | std::views::keys)
            {
                if (int w = key; colors[w] == -1)
                {
                    pending_vertices.push(w);
                }
            }
        }
    }

    // The greedy implementation uses zero-based colors internally, whereas
    // the SAT encodings expose the positive domain [1, upper_bound].
    upper_bound = max_color + 1;
}


BCPSolver::BCPSolver::BCPSolver(const Graph* graph, const int upper_bound,
                                const bool use_symmetry_breaking,
                                const bool use_heuristic)
    : graph(graph), upper_bound(upper_bound),
      use_symmetry_breaking(use_symmetry_breaking),
      use_heuristic(use_heuristic)
{
    sat_solver = std::make_unique<SATSolver::Cadical>();

    if (this->upper_bound < 0)
    {
        calculate_upper_bound();
    }

    lower_bound = graph->get_number_of_nodes() == 0 ? 0 : 1;
    if (this->upper_bound < lower_bound)
    {
        throw std::invalid_argument("Upper bound is below the positive-color lower bound.");
    }

    span = this->upper_bound;
}

BCPSolver::BCPSolver* BCPSolver::BCPSolver::create_solver(const SolvingMethod method,
                                                          const Graph* graph,
                                                          const int upper_bound,
                                                          const bool use_symmetry_breaking,
                                                          const bool use_heuristic,
                                                          const std::string& width)
{
    switch (method)
    {
    case TwoVariablesGreater:
        if (!width.empty())
        {
            throw std::invalid_argument("TwoVariablesGreater method does not support width parameter");
        }
        return new TwoVarsGreaterMethod(graph, upper_bound, use_symmetry_breaking, use_heuristic);
    case TwoVariablesLess:
        if (!width.empty())
        {
            throw std::invalid_argument("TwoVariablesLess method does not support width parameter");
        }
        return new TwoVarsLessMethod(graph, upper_bound, use_symmetry_breaking, use_heuristic);
    case OneVariableGreater:
        if (!width.empty())
        {
            throw std::invalid_argument("OneVariableGreater method does not support width parameter");
        }
        return new OneVarGreaterMethod(graph, upper_bound, use_symmetry_breaking, use_heuristic);
    case OneVariableLess:
        if (!width.empty())
        {
            throw std::invalid_argument("OneVariableLess method does not support width parameter");
        }
        return new OneVarLessMethod(graph, upper_bound, use_symmetry_breaking, use_heuristic);
    case StaircaseWithAuxiliaryVarsNoCache:
        if (width.empty())
        {
            throw std::invalid_argument("StaircaseWithAuxiliaryVarsNoCache method requires width parameter");
        }
        return new StaircaseWithAuxiliaryVarsMethod(graph, upper_bound, use_symmetry_breaking, use_heuristic,
                                                    false, width);
    case StaircaseWithAuxiliaryVarsWithCache:
        if (width.empty())
        {
            throw std::invalid_argument("StaircaseWithAuxiliaryVarsWithCache method requires width parameter");
        }
        return new StaircaseWithAuxiliaryVarsMethod(graph, upper_bound, use_symmetry_breaking, use_heuristic,
                                                    true, width);
    case StaircaseWithoutAuxiliaryVars:
        if (width.empty())
        {
            throw std::invalid_argument("StaircaseWithoutAuxiliaryVars method requires width parameter");
        }
        return new StaircaseWithoutAuxiliaryVarsMethod(graph, upper_bound, use_symmetry_breaking,
                                                       use_heuristic, width);
    default:
        throw std::invalid_argument("Invalid solving method");
    }
}

BCPSolver::SolverStatus BCPSolver::BCPSolver::non_optimal_solving(const double time_limit)
{
    timed_out = false;
    optimality_proven = false;
    encode();

    double remaining_time = time_limit;
    if (time_limit != NO_TIME_LIMIT)
    {
        remaining_time -= encoding_time + sat_solver->get_statistics()["total_solving_time"];
        if (remaining_time <= 0.0)
        {
            timed_out = true;
            status = UNKNOWN;
            return status;
        }
    }

    if (const int result = sat_solver->solve(nullptr, remaining_time); result == CaDiCaL::Status::UNKNOWN)
    {
        timed_out = time_limit != NO_TIME_LIMIT;
        status = UNKNOWN;
        return status;
    }
    else
    {
        status = result == CaDiCaL::Status::SATISFIABLE ? SATISFIABLE : UNSATISFIABLE;
        return status;
    }
}

BCPSolver::SolverStatus BCPSolver::BCPSolver::optimal_solving_non_incremental(const double time_limit)
{
    const SolverStatus initial_result = non_optimal_solving(time_limit);

    if (initial_result == UNKNOWN)
    {
        return status;
    }

    if (initial_result == UNSATISFIABLE)
    {
        return status;
    }

    while (span > lower_bound)
    {
        sat_solver->reset();
        span--;
        encode();

        int result;
        if (time_limit == NO_TIME_LIMIT)
        {
            result = sat_solver->solve();
        }
        else
        {
            const auto remaining_time = time_limit - encoding_time - sat_solver->get_statistics()["total_solving_time"];
            if (remaining_time <= 0.0)
            {
                span++;
                status = SATISFIABLE;
                timed_out = true;
                return status;
            }
            result = sat_solver->solve(nullptr, remaining_time);
        }

        if (result == CaDiCaL::Status::SATISFIABLE)
        {
            continue;
        }

        span++;
        if (result == CaDiCaL::Status::UNSATISFIABLE)
        {
            status = OPTIMAL;
            optimality_proven = true;
        }
        else
        {
            status = SATISFIABLE;
            timed_out = time_limit != NO_TIME_LIMIT;
        }
        return status;
    }

    status = OPTIMAL;
    optimality_proven = true;
    return status;
}

BCPSolver::SolverStatus BCPSolver::BCPSolver::optimal_solving_incremental(
    const double time_limit, const std::string& variable_for_incremental)
{
    const SolverStatus initial_result = non_optimal_solving(time_limit);

    if (initial_result == UNKNOWN)
    {
        return status;
    }

    if (initial_result == UNSATISFIABLE)
    {
        return status;
    }

    while (span > lower_bound)
    {
        const auto tightening_start = std::chrono::high_resolution_clock::now();
        const auto literals = create_bound_tightening_literals(variable_for_incremental);

        for (const int literal : literals)
        {
            sat_solver->add_clause(literal);
        }
        encoding_time += std::chrono::duration<double>(
            std::chrono::high_resolution_clock::now() - tightening_start).count();

        int result;
        if (time_limit == NO_TIME_LIMIT)
        {
            result = sat_solver->solve();
        }
        else
        {
            const auto remaining_time = time_limit - encoding_time - sat_solver->get_statistics()["total_solving_time"];
            if (remaining_time <= 0.0)
            {
                status = SATISFIABLE;
                timed_out = true;
                return status;
            }
            result = sat_solver->solve(nullptr, remaining_time);
        }

        if (result == CaDiCaL::Status::SATISFIABLE)
        {
            span--;
            continue;
        }

        if (result == CaDiCaL::Status::UNSATISFIABLE)
        {
            status = OPTIMAL;
            optimality_proven = true;
        }
        else
        {
            status = SATISFIABLE;
            timed_out = time_limit != NO_TIME_LIMIT;
        }
        return status;
    }

    status = OPTIMAL;
    optimality_proven = true;
    return status;
}

BCPSolver::SolverStatus BCPSolver::BCPSolver::solve(const double time_limit, const bool find_optimal,
                                                    const bool incremental, const std::string& variable_for_incremental)
{
    if (!find_optimal)
    {
        return non_optimal_solving(time_limit);
    }
    else
    {
        if (!incremental)
        {
            return optimal_solving_non_incremental(time_limit);
        }

        else
        {
            return optimal_solving_incremental(time_limit, variable_for_incremental);
        }
    }
}

int BCPSolver::BCPSolver::get_span() const
{
    return (status != UNKNOWN && status != UNSATISFIABLE) ? span : -1;
}

std::unordered_map<std::string, double> BCPSolver::BCPSolver::get_statistics() const
{
    auto stats = sat_solver->get_statistics();

    stats["V"] = graph->get_number_of_nodes();
    stats["E"] = graph->get_number_of_edges();
    stats["upper_bound"] = upper_bound;
    stats["status"] = status;
    stats["span"] = get_span();
    stats["encoding_time"] = encoding_time;
    stats["timed_out"] = timed_out ? 1.0 : 0.0;
    stats["optimality_proven"] = optimality_proven ? 1.0 : 0.0;

    stats["time_used"] = encoding_time + stats["total_solving_time"];

    return stats;
}

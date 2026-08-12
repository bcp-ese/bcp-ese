//
// Created by homulily85 on 12/30/25.
//

#include <cmath>
#include "StaircaseWithAuxiliaryVarsMethod.h"

void BCPSolver::StaircaseWithAuxiliaryVarsMethod::symmetry_breaking()
{
    if (graph->get_number_of_nodes() == 0)
    {
        return;
    }
    const int midpoint = (span + 1) / 2;
    for (int c = midpoint + 1; c < span + 1; c++)
    {
        sat_solver->add_clause(-x[{graph->get_highest_degree_vertex(), c}]);
    }
}

void BCPSolver::StaircaseWithAuxiliaryVarsMethod::first_constraint()
{
    if (width == "vary")
    {
        for (int i = 0; i < graph->get_number_of_nodes(); i++)
        {
            if (graph->get_degree(i) == 0)
            {
                std::vector<int> vars;
                for (int c = 1; c < span + 1; c++)
                {
                    vars.push_back(x[{i, c}]);
                }
                sat_solver->encode_equals_k(vars, 1);
                continue;
            }

            int max_weight_of_current_node = 0;
            for (const auto& neighbor : graph->get_neighbors(i))
            {
                if (const int weight = graph->get_weight(i, neighbor); weight > max_weight_of_current_node)
                {
                    max_weight_of_current_node = weight;
                }
            }

            max_weight[i] = max_weight_of_current_node;

            if (max_weight_of_current_node <= 1)
            {
                std::vector<int> vars;
                for (int c = 1; c < span + 1; c++)
                {
                    vars.push_back(x[{i, c}]);
                }
                sat_solver->encode_equals_k(vars, 1);
            }
            else
            {
                const auto groups = group_range(span, max_weight_of_current_node);
                encode_staircase_block(i, 1, span, max_weight_of_current_node);
                std::vector<int> aux_vars;
                for (const auto& [fst, snd] : groups)
                {
                    aux_vars.push_back(get_aux_var_for_staircase(i, fst, snd));
                }
                sat_solver->encode_equals_k(aux_vars, 1);
            }
        }
    }
    else if (width == "fixed")
    {
        int max_weight_global = 0;
        for (const auto& [u, v, weight] : graph->get_edges())
        {
            if (weight > max_weight_global)
            {
                max_weight_global = weight;
            }
        }
        for (int i = 0; i < graph->get_number_of_nodes(); i++)
        {
            if (graph->get_degree(i) == 0)
            {
                std::vector<int> vars;
                for (int c = 1; c < span + 1; c++)
                {
                    vars.push_back(x[{i, c}]);
                }
                sat_solver->encode_equals_k(vars, 1);
                continue;
            }

            const int max_weight_of_current_node = max_weight_global;

            max_weight[i] = max_weight_global;

            const auto groups = group_range(span, max_weight_of_current_node);
            encode_staircase_block(i, 1, span, max_weight_of_current_node);
            std::vector<int> aux_vars;
            for (const auto& [fst, snd] : groups)
            {
                aux_vars.push_back(get_aux_var_for_staircase(i, fst, snd));
            }
            sat_solver->encode_equals_k(aux_vars, 1);
        }
    }
}

// void BCPSolver::StaircaseWithAuxiliaryVarsMethod::first_constraint()
// {
//     int max_weight_global = 0;
//     for (const auto& [u, v, weight] : graph->get_edges())
//     {
//         if (weight > max_weight_global)
//         {
//             max_weight_global = weight;
//         }
//     }
//     for (int i = 0; i < graph->get_number_of_nodes(); i++)
//     {
//         if (graph->get_degree(i) == 0)
//         {
//             std::vector<int> vars;
//             for (int c = 1; c < span + 1; c++)
//             {
//                 vars.push_back(x[{i, c}]);
//             }
//             sat_solver->encode_equals_k(vars, 1);
//             continue;
//         }
//
//         // int max_weight_of_current_node = 0;
//         int max_weight_of_current_node = max_weight_global;
//         // for (const auto& neighbor : graph->get_neighbors(i))
//         // {
//         //     if (const int weight = graph->get_weight(i, neighbor); weight > max_weight_of_current_node)
//         //     {
//         //         max_weight_of_current_node = weight;
//         //     }
//         // }
//
//         // max_weight[i] = max_weight_of_current_node;
//
//         max_weight[i] = max_weight_global;
//
//         // if (max_weight_of_current_node <= 1)
//         // {
//         //     std::vector<int> vars;
//         //     for (int c = 1; c < span + 1; c++)
//         //     {
//         //         vars.push_back(x[{i, c}]);
//         //     }
//         //     sat_solver.encode_equals_k(vars, 1);
//         // }
//         // else
//         {
//             const auto groups = group_range(span, max_weight_of_current_node);
//             encode_staircase_block(i, 1, span, max_weight_of_current_node);
//             std::vector<int> aux_vars;
//             for (const auto& [fst, snd] : groups)
//             {
//                 aux_vars.push_back(get_aux_var_for_staircase(i, fst, snd));
//             }
//             sat_solver->encode_equals_k(aux_vars, 1);
//         }
//     }
// }

void BCPSolver::StaircaseWithAuxiliaryVarsMethod::second_constraint()
{
    used_tuple.clear();
    for (const auto& [u, v, weight] : graph->get_edges())
    {
        for (int c = 1; c <= span && c < weight; c++)
        {
            if (c - weight < 1 && c + weight - 1 > span)
            {
                sat_solver->add_clause(-x[{u, c}]);
            }
        }

        if (use_heuristic)
        {
            if (weight == 1)
            {
                for (int c = 1; c < span + 1; c++)
                {
                    sat_solver->add_clause(-x[{u, c}], -x[{v, c}]);
                }
                continue;
            }
        }

        for (int c = 1; c < span - weight + 2; c++)
        {
            const auto groups_for_u = split_range_by_groups(c, c + weight - 1, max_weight[u]);
            const auto vars_for_u = get_var_for_groups(u, groups_for_u);
            const auto groups_for_v = split_range_by_groups(c, c + weight - 1, max_weight[v]);
            const auto vars_for_v = get_var_for_groups(v, groups_for_v);

            for (const auto var_u : vars_for_u)
            {
                for (const auto var_v : vars_for_v)
                {
                    sat_solver->add_clause(-var_u, -var_v);
                }
            }
        }
    }
}

void BCPSolver::StaircaseWithAuxiliaryVarsMethod::encode()
{
    const auto start_time = std::chrono::high_resolution_clock::now();

    create_variable();

    if (use_symmetry_breaking)
    {
        symmetry_breaking();
    }

    first_constraint();
    second_constraint();

    encoding_time += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start_time).count();
}

void BCPSolver::StaircaseWithAuxiliaryVarsMethod::create_variable()
{
    x.clear();
    staircase_aux_vars.clear();

    for (int i = 0; i < graph->get_number_of_nodes(); i++)
    {
        for (int c = 1; c < span + 1; c++)
        {
            x.insert(std::pair<std::pair<int, int>, int>({i, c}, sat_solver->create_new_variable()));
        }
    }
}

std::vector<int> BCPSolver::StaircaseWithAuxiliaryVarsMethod::create_bound_tightening_literals(
    const std::string& variable_for_incremental)
{
    if (variable_for_incremental == "x")
    {
        std::vector<int> literals(graph->get_number_of_nodes());

        for (int i = 0; i < graph->get_number_of_nodes(); i++)
        {
            literals[i] = -x.at({i, span});
        }
        return literals;
    }
    else
    {
        throw std::runtime_error("Invalid variable for incremental in StaircaseMethod.");
    }
}

int BCPSolver::StaircaseWithAuxiliaryVarsMethod::get_aux_var_for_staircase(
    const int node, const int start, const int end)
{
    if (start > end)
    {
        throw std::runtime_error(
            "Invalid range for auxiliary variable" + std::to_string(start) + " to " + std::to_string(end));
    }

    if (start == end)
    {
        return x[{node, start}];
    }

    const auto key = std::make_tuple(node, start, end);
    if (const auto it = staircase_aux_vars.find(key); it != staircase_aux_vars.end())
    {
        return it->second;
    }

    const int aux_var = sat_solver->create_new_variable();
    staircase_aux_vars[key] = aux_var;

    return aux_var;
}

void BCPSolver::StaircaseWithAuxiliaryVarsMethod::encode_staircase_block(
    const int node, const int block_start, const int block_end, int block_width)
{
    const int n = block_end - block_start + 1;
    if (n < block_width)
    {
        block_width = n;
    }
    if (n == 1)
    {
        return;
    }

    for (int i = 0; i < static_cast<int>(ceil(n / static_cast<double>(block_width))); i++)
    {
        encode_window(node, i, n, block_width);
    }

    for (int i = 0; i < static_cast<int>(ceil(n / static_cast<double>(block_width))) - 1; i++)
    {
        glue_window(node, i, n, block_width);
    }
}

void BCPSolver::StaircaseWithAuxiliaryVarsMethod::encode_window(const int node, const int window_index, const int n,
                                                                const int block_width)
{
    if (window_index == 0)
    {
        const int last_color = (window_index + 1) * block_width;

        for (int i = 2; i < block_width + 1; i++)
        {
            int current_color = (window_index + 1) * block_width + 1 - i;
            sat_solver->add_clause(
                -x[{node, current_color}],
                get_aux_var_for_staircase(node, current_color, last_color));

            sat_solver->add_clause(
                -get_aux_var_for_staircase(node, current_color + 1, last_color),
                get_aux_var_for_staircase(node, current_color, last_color));

            sat_solver->add_clause(
                x[{node, current_color}],
                get_aux_var_for_staircase(node, current_color + 1, last_color),
                -get_aux_var_for_staircase(node, current_color, last_color));
        }

        for (int i = 2; i < block_width + 1; i++)
        {
            int current_color = (window_index + 1) * block_width + 1 - i;
            sat_solver->add_clause(-x[{node, current_color}],
                                   -get_aux_var_for_staircase(node, current_color + 1, last_color));
        }
    }
    else if (window_index == static_cast<int>(ceil(n / static_cast<double>(block_width)) - 1))
    {
        const int first_color = window_index * block_width + 1;
        if ((window_index + 1) * block_width > n)
        {
            const int real_width = n % block_width;

            for (int i = 2; i < real_width + 1; i++)
            {
                const int current_color = window_index * block_width + i;

                sat_solver->add_clause(
                    -x[{node, current_color}],
                    get_aux_var_for_staircase(node, first_color, current_color));

                sat_solver->add_clause(
                    -get_aux_var_for_staircase(node, first_color, current_color - 1),
                    get_aux_var_for_staircase(node, first_color, current_color));

                sat_solver->add_clause(
                    -x[{node, current_color}],
                    -get_aux_var_for_staircase(node, first_color, current_color - 1));

                sat_solver->add_clause(
                    x[{node, current_color}],
                    get_aux_var_for_staircase(node, first_color, current_color - 1),
                    -get_aux_var_for_staircase(node, first_color, current_color));
            }
        }
        else
        {
            for (int i = 2; i < block_width + 1; i++)
            {
                const int current_color = window_index * block_width + i;

                sat_solver->add_clause(
                    -x[{node, current_color}],
                    get_aux_var_for_staircase(node, first_color, current_color));

                sat_solver->add_clause(
                    -get_aux_var_for_staircase(node, first_color, current_color - 1),
                    get_aux_var_for_staircase(node, first_color, current_color));

                sat_solver->add_clause(
                    x[{node, current_color}],
                    get_aux_var_for_staircase(node, first_color, current_color - 1),
                    -get_aux_var_for_staircase(node, first_color, current_color));
            }

            for (int i = 2; i < block_width + 1; i++)
            {
                const int current_color = window_index * block_width + i;
                sat_solver->add_clause(
                    -x[{node, current_color}],
                    -get_aux_var_for_staircase(node, first_color, current_color - 1));
            }
        }
    }

    else
    {
        const int first_color = window_index * block_width + 1;

        for (int i = 2; i < block_width + 1; i++)
        {
            const int current_color = window_index * block_width + i;

            sat_solver->add_clause(
                -x[{node, current_color}],
                get_aux_var_for_staircase(node, first_color, current_color));

            sat_solver->add_clause(
                -get_aux_var_for_staircase(node, first_color, current_color - 1),
                get_aux_var_for_staircase(node, first_color, current_color));

            sat_solver->add_clause(
                x[{node, current_color}],
                get_aux_var_for_staircase(node, first_color, current_color - 1),
                -get_aux_var_for_staircase(node, first_color, current_color));
        }

        for (int i = 2; i < block_width + 1; i++)
        {
            const int current_color = window_index * block_width + i;
            sat_solver->add_clause(
                -x[{node, current_color}],
                -get_aux_var_for_staircase(node, first_color, current_color - 1));
        }

        const int last_color = (window_index + 1) * block_width;

        for (int i = 2; i < block_width + 1; i++)
        {
            int current_color = (window_index + 1) * block_width + 1 - i;

            sat_solver->add_clause(
                -x[{node, current_color}],
                get_aux_var_for_staircase(node, current_color, last_color));

            sat_solver->add_clause(
                -get_aux_var_for_staircase(node, current_color + 1, last_color),
                get_aux_var_for_staircase(node, current_color, last_color));

            sat_solver->add_clause(
                x[{node, current_color}],
                get_aux_var_for_staircase(node, current_color + 1, last_color),
                -get_aux_var_for_staircase(node, current_color, last_color));
        }
    }
}

void BCPSolver::StaircaseWithAuxiliaryVarsMethod::glue_window(const int node, const int window_index, const int n,
                                                              const int block_width)
{
    const int last_color = (window_index + 1) * block_width;
    const int first_color_reverse = (window_index + 1) * block_width + 1;

    if ((window_index + 2) * block_width > n)
    {
        const int real_width = n % block_width;
        for (int i = 1; i < real_width + 1; i++)
        {
            const int current_color = window_index * block_width + i + 1;
            const int current_reverse_color = (window_index + 1) * block_width + i;

            sat_solver->add_clause(
                -get_aux_var_for_staircase(node, current_color, last_color),
                -get_aux_var_for_staircase(node, first_color_reverse, current_reverse_color));
        }
    }
    else
    {
        for (int i = 1; i < block_width; i++)
        {
            const int current_color = window_index * block_width + i + 1;
            const int current_reverse_color = (window_index + 1) * block_width + i;

            sat_solver->add_clause(
                -get_aux_var_for_staircase(node, current_color, last_color),
                -get_aux_var_for_staircase(node, first_color_reverse, current_reverse_color));
        }
    }
}

std::vector<std::pair<int, int>> BCPSolver::StaircaseWithAuxiliaryVarsMethod::group_range(const int n, const int k)
{
    std::vector<std::pair<int, int>> ranges;
    for (int i = 1; i <= n; i += k)
    {
        ranges.emplace_back(i, std::min(i + k - 1, n));
    }

    return ranges;
}

std::vector<std::pair<int, int>> BCPSolver::StaircaseWithAuxiliaryVarsMethod::split_range_by_groups(
    const int a, const int b, const int k)
{
    std::vector<std::pair<int, int>> result;
    int current = a;

    while (current <= b)
    {
        const int group_start = ((current - 1) / k) * k + 1;
        const int group_end = group_start + k - 1;

        const int part_start = current;
        const int part_end = std::min(b, group_end);

        result.emplace_back(part_start, part_end);

        current = part_end + 1;
    }
    return result;
}

int BCPSolver::StaircaseWithAuxiliaryVarsMethod::find_group_bound(const int a, const int b, const int k, const int r)
{
    std::vector<std::pair<int, int>> groups;
    int start = 1;
    while (start <= r)
    {
        const int end = std::min(start + k - 1, r);
        groups.emplace_back(start, end);
        start = end + 1;
    }

    // Case 1: [a,b] contains both lb and ub of a group
    for (const auto& [lb, ub] : groups)
    {
        if (a <= lb && b >= ub)
        {
            return ub;
        }
    }

    // Case 2: [a,b] contains only lb (but not ub) of a group
    for (const auto& [lb, ub] : groups)
    {
        if (a <= lb && lb <= b && b < ub)
        {
            return lb;
        }
    }

    // Case 3: [a,b] doesn't contain complete lb or lb+ub
    // Find the group that [a,b] overlaps with or is nearest to
    for (const auto& [lb, ub] : groups)
    {
        if ((lb <= a && a <= ub) || (lb <= b && b <= ub))
        {
            return ub;
        }
    }

    auto nearest_group = groups[0];
    int nearest_distance = std::min(std::abs(nearest_group.first - a), std::abs(nearest_group.second - b));
    for (const auto& g : groups)
    {
        if (const int distance = std::min(std::abs(g.first - a), std::abs(g.second - b)); distance < nearest_distance)
        {
            nearest_distance = distance;
            nearest_group = g;
        }
    }
    return nearest_group.second;
}

std::vector<int> BCPSolver::StaircaseWithAuxiliaryVarsMethod::create_aux_var_for_groups_backward(const int node,
    const std::vector<std::pair<int, int>>& group, const int bound)
{
    std::vector<int> aux_vars;
    const int s = sat_solver->create_new_variable();
    sat_solver->add_clause(
        get_aux_var_for_staircase(node, group[0].first, bound),
        -get_aux_var_for_staircase(node, group[0].second + 1, bound),
        s);
    sat_solver->add_clause(
        -get_aux_var_for_staircase(node, group[0].first, bound),
        get_aux_var_for_staircase(node, group[0].second + 1, bound),
        s);
    sat_solver->add_clause(
        -s, get_aux_var_for_staircase(node, group[0].first, bound));
    sat_solver->add_clause(
        -s, -get_aux_var_for_staircase(node, group[0].second + 1, bound));

    aux_vars.push_back(s);

    return aux_vars;
}

std::vector<int> BCPSolver::StaircaseWithAuxiliaryVarsMethod::create_aux_var_for_groups_forward(const int node,
    const std::vector<std::pair<int, int>>& group, const int bound)
{
    std::vector<int> aux_vars;
    const int s = sat_solver->create_new_variable();
    sat_solver->add_clause(
        get_aux_var_for_staircase(node, bound, group[0].second),
        -get_aux_var_for_staircase(node, bound, group[0].first - 1),
        s);
    sat_solver->add_clause(
        -get_aux_var_for_staircase(node, bound, group[0].second),
        get_aux_var_for_staircase(node, bound, group[0].first - 1),
        s);
    sat_solver->add_clause(
        -s, get_aux_var_for_staircase(node, bound, group[0].second));
    sat_solver->add_clause(
        -s, -get_aux_var_for_staircase(node, bound, group[0].first - 1));

    aux_vars.push_back(s);

    return aux_vars;
}

std::vector<int> BCPSolver::StaircaseWithAuxiliaryVarsMethod::get_var_for_groups(const int node,
    const std::vector<std::pair<int, int>>& group)
{
    if (group.size() == 1)
    {
        if (use_cache)
        {
            const int bound = find_group_bound(group[0].first, group[0].second, max_weight[node], span);
            const auto tuple_key = std::make_tuple(node, group[0].first, group[0].second, bound);

            if (used_tuple.contains(tuple_key))
            {
                return {used_tuple[tuple_key]};
            }

            if (group[0].second < max_weight[node])
            {
                const int new_bound = max_weight[node];
                used_tuple[tuple_key] = create_aux_var_for_groups_backward(node, group, new_bound)[0];
                return {used_tuple[tuple_key]};
            }
            if ((span - 1) / max_weight[node] * max_weight[node] + 1 < group[0].first)
            {
                const int new_bound = ((span - 1) / max_weight[node]) * max_weight[node] + 1;
                used_tuple[tuple_key] = create_aux_var_for_groups_forward(node, group, new_bound)[0];
                return {used_tuple[tuple_key]};
            }
            if (bound != group[0].first && bound != group[0].second)
            {
                used_tuple[tuple_key] = create_aux_var_for_groups_backward(node, group, bound)[0];
                return {used_tuple[tuple_key]};
            }
        }
        else
        {
            const int bound = find_group_bound(group[0].first, group[0].second, max_weight[node], span);

            if (group[0].second < max_weight[node])
            {
                const int new_bound = max_weight[node];
                return create_aux_var_for_groups_backward(node, group, new_bound);
            }
            if ((span - 1) / max_weight[node] * max_weight[node] + 1 < group[0].first)
            {
                const int new_bound = ((span - 1) / max_weight[node]) * max_weight[node] + 1;
                return create_aux_var_for_groups_forward(node, group, new_bound);
            }
            if (bound != group[0].first && bound != group[0].second)
            {
                return create_aux_var_for_groups_backward(node, group, bound);
            }
        }
        return {get_aux_var_for_staircase(node, group[0].first, group[0].second)};
    }

    return {
        get_aux_var_for_staircase(node, group[0].first, group[0].second),
        get_aux_var_for_staircase(node, group[1].first, group[1].second)
    };
}

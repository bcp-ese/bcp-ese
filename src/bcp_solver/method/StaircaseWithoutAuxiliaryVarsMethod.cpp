//
// Created by homulily85 on 1/4/26.
//

#include "StaircaseWithoutAuxiliaryVarsMethod.h"

namespace BCPSolver
{
    void StaircaseWithoutAuxiliaryVarsMethod::second_constraint()
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

            // 3. Sliding Window Constraints
            // Python: range(1, self._span - weight + 2) -> [1, span - weight + 1] inclusive
            for (int c = 1; c <= span - weight + 1; c++)
            {
                const int range_start = c;
                const int range_end = c + weight - 1;

                // Get variables (literals or pairs) for the windows
                auto vars_u = get_vars_for_constraint_group(u, range_start, range_end);
                auto vars_v = get_vars_for_constraint_group(v, range_start, range_end);

                // Cartesian Product: u vs v
                for (const auto& var_u : vars_u)
                {
                    for (const auto& var_v : vars_v)
                    {
                        std::vector<int> clause;

                        // Helper lambda to expand negative of literal/pair
                        // If pair is (A, B) representing (A AND NOT B),
                        // then NEGATION is (NOT A OR B) -> add -A, add B.
                        auto expand_negative = [&](const std::pair<int, int>& lit)
                        {
                            if (lit.first != 0) clause.push_back(-lit.first); // -A
                            if (lit.second != 0) clause.push_back(lit.second); // +B
                        };

                        expand_negative(var_u);
                        expand_negative(var_v);

                        if (!clause.empty())
                        {
                            sat_solver->add_clause(clause);
                        }
                    }
                }
            }
        }
    }

    std::vector<std::pair<int, int>> StaircaseWithoutAuxiliaryVarsMethod::get_vars_for_constraint_group(
        const int node, const int start, const int end)
    {
        // 1. Split range by groups
        const auto groups = split_range_by_groups(start, end, max_weight[node]);

        // 2. If range maps to multiple underlying groups, return all corresponding aux vars
        if (groups.size() != 1)
        {
            std::vector<std::pair<int, int>> result;
            for (const auto& g : groups)
            {
                // Simple variable returned as pair {var, 0}
                result.emplace_back(get_aux_var_for_staircase(node, g.first, g.second), 0);
            }
            return result;
        }

        // 3. Handle single group logic
        const int g_start = groups[0].first;
        const int g_end = groups[0].second;
        const int bound = find_group_bound(g_start, g_end, max_weight[node], span);
        const int max_w = max_weight[node];

        // Check Cache
        // ensure 'used_tuple' maps to std::pair<int, int>
        const auto tuple_key = std::make_tuple(node, g_start, g_end, bound);
        if (used_tuple.contains(tuple_key))
        {
            return {used_tuple[tuple_key]};
        }

        std::pair<int, int> var_pair;

        // Case: Range is at the START of a block -> use Backward Difference
        if (g_end < max_w)
        {
            // Python: _create_backward_difference_var(node, g_start, g_end, max_w)
            // Returns (upper, lower) -> (staircase(..., g_start, bound), staircase(..., g_end + 1, bound))
            // Here bound passed is max_w
            var_pair = {
                get_aux_var_for_staircase(node, g_start, max_w),
                get_aux_var_for_staircase(node, g_end + 1, max_w)
            };
        }
        // Case: Range is at the END of the LAST block -> use Forward Difference
        else if (((span - 1) / max_w) * max_w + 1 < g_start)
        {
            int last_block_start = ((span - 1) / max_w) * max_w + 1;
            // Python: _create_forward_difference_var(node, g_start, g_end, last_block_start)
            // Returns (upper, lower) -> (staircase(..., bound, g_end), staircase(..., bound, g_start - 1))
            var_pair = {
                get_aux_var_for_staircase(node, last_block_start, g_end),
                get_aux_var_for_staircase(node, last_block_start, g_start - 1)
            };
        }
        // Case: Range is inside a middle block -> use Backward Difference
        else if (bound != g_start && bound != g_end)
        {
            // Python: _create_backward_difference_var(node, g_start, g_end, bound)
            var_pair = {
                get_aux_var_for_staircase(node, g_start, bound),
                get_aux_var_for_staircase(node, g_end + 1, bound)
            };
        }
        // Case: Exact match (Range == Block)
        else
        {
            // Return immediately, usually not cached in difference map
            return {{get_aux_var_for_staircase(node, g_start, g_end), 0}};
        }

        // Cache and return
        used_tuple[tuple_key] = var_pair;
        return {var_pair};
    }
} // BCPSolver

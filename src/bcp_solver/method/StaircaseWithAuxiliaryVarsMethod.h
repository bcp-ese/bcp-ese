//
// Created by homulily85 on 12/30/25.
//

#ifndef BCP_STAIRCASEWITHAUXILIARYVARSMETHOD_H
#define BCP_STAIRCASEWITHAUXILIARYVARSMETHOD_H

#include "bcp_solver/bcp_solver.h"

namespace BCPSolver
{
    class StaircaseWithoutAuxiliaryVarsMethod;

    class StaircaseWithAuxiliaryVarsMethod : public BCPSolver
    {
    protected:
        std::map<std::tuple<int, int, int>, int> staircase_aux_vars{};
        std::map<std::tuple<int, int, int, int>, int> used_tuple;
        std::vector<int> max_weight{std::vector(graph->get_number_of_nodes(), 0)};
        const std::string width;

        bool use_cache{};

        void symmetry_breaking();

        void first_constraint();

        virtual void second_constraint();

        void encode() override;

        void create_variable() override;

        std::vector<int> create_bound_tightening_literals(
            const std::string& variable_for_incremental) override;

        friend class BCPSolver;

        explicit StaircaseWithAuxiliaryVarsMethod(const Graph* graph,
                                                  const int upper_bound,
                                                  const bool use_symmetry_breaking,
                                                  const bool use_heuristic,
                                                  const bool use_cache,
                                                  const std::string& width) :
            BCPSolver(graph, upper_bound, use_symmetry_breaking, use_heuristic), width(width),
            use_cache(use_cache)
        {
        }

        int get_aux_var_for_staircase(int node, int start, int end);

        void encode_staircase_block(int node, int block_start, int block_end, int block_width);

        void encode_window(int node, int window_index, int n, int block_width);

        void glue_window(int node, int window_index, int n, int block_width);

        static std::vector<std::pair<int, int>> group_range(int n, int k);

        static std::vector<std::pair<int, int>> split_range_by_groups(int a, int b, int k);

        static int find_group_bound(int a, int b, int k, int r);

        std::vector<int>
        create_aux_var_for_groups_backward(int node, const std::vector<std::pair<int, int>>& group, int bound);

        std::vector<int>
        create_aux_var_for_groups_forward(int node, const std::vector<std::pair<int, int>>& group, int bound);

        std::vector<int> get_var_for_groups(int node, const std::vector<std::pair<int, int>>& group);
    };
}


#endif //BCP_STAIRCASEWITHAUXILIARYVARSMETHOD_H

//
// Created by homulily85 on 12/30/25.
//

#ifndef BCP_ONEVARLESSMETHOD_H
#define BCP_ONEVARLESSMETHOD_H
#include "bcp_solver/bcp_solver.h"


namespace BCPSolver
{
    class OneVarLessMethod : public BCPSolver
    {
    private:
        void symmetry_breaking();

        void first_constraint();

        void second_constraint();

        void third_constraint();

        void encode() override;

        void create_variable() override;

        std::vector<int> create_bound_tightening_literals(
            const std::string& variable_for_incremental) override;

        friend class BCPSolver;

        explicit OneVarLessMethod(const Graph* graph, const int upper_bound,
                                  const bool use_symmetry_breaking,
                                  const bool use_heuristic) : BCPSolver(
            graph, upper_bound, use_symmetry_breaking, use_heuristic)
        {
            if (use_heuristic)
            {
                throw std::runtime_error("Pairwise encoding is not supported in OneVarLessMethod.");
            }
        }
    };
}


#endif //BCP_ONEVARLESSMETHOD_H

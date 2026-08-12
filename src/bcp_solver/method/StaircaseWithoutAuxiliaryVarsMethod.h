//
// Created by homulily85 on 1/4/26.
//

#ifndef BCP_STAIRCASEWITHOUTAUXILIARYVARSMETHOD_H
#define BCP_STAIRCASEWITHOUTAUXILIARYVARSMETHOD_H
#include "StaircaseWithAuxiliaryVarsMethod.h"
#include "bcp_solver/bcp_solver.h"

namespace BCPSolver
{
    class StaircaseWithoutAuxiliaryVarsMethod : public StaircaseWithAuxiliaryVarsMethod
    {
    private:
        friend class BCPSolver;
        std::map<std::tuple<int, int, int, int>, std::pair<int, int>> used_tuple;

        explicit StaircaseWithoutAuxiliaryVarsMethod(const Graph* graph, const int upper_bound,
                                                     const bool use_symmetry_breaking,
                                                     const bool use_heuristic, const std::string& width) :
            StaircaseWithAuxiliaryVarsMethod(graph, upper_bound, use_symmetry_breaking, use_heuristic, false,
                                             width)
        {
        }

    protected:
        void second_constraint() override;
        std::vector<std::pair<int, int>> get_vars_for_constraint_group(int node, int start, int end);
    };
} // BCPSolver

#endif //BCP_STAIRCASEWITHOUTAUXILIARYVARSMETHOD_H

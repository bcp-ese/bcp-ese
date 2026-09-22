//
// Created by homulily85 on 1/16/26.
//

#ifndef BCP_KISSAT_H
#define BCP_KISSAT_H
#include "SatSolver.h"

namespace SATSolver
{
    class Kissat : public SatSolver
    {
    private:
        std::vector<std::vector<int>> clauses;

        static std::string get_random_filename();

        [[nodiscard]] std::string write_cnf_to_file() const;

    public:
        Kissat() = default;

        ~Kissat() override = default;

        void add_clause(const std::vector<int>& clause) override;

        void add_clause(int l) override;

        void add_clause(int l1, int l2) override;

        void add_clause(int l1, int l2, int l3) override;

        void add_clause(int l1, int l2, int l3, int l4) override;

        int solve(const std::vector<int>* assumptions, double time_limit) override;

        void reset() override;
    };
} // SatSolver

#endif //BCP_KISSAT_H

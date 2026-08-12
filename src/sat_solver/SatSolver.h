//
// Created by homulily85 on 11/29/25.
//

#ifndef BCP_SATSOLVER_H
#define BCP_SATSOLVER_H

#include <chrono>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

static constexpr double NO_TIME_LIMIT = std::numeric_limits<double>::lowest();

namespace SATSolver
{
    class SatSolver
    {
    protected:
        int number_of_clauses{};
        int number_of_variables{};
        int status{};
        double time_accum{};

    public:
        SatSolver() = default;

        virtual ~SatSolver()=default;

        [[nodiscard]] int create_new_variable();

        virtual void add_clause(const std::vector<int>& clause)=0;

        virtual void add_clause(int l)=0;

        virtual void add_clause(int l1, int l2)=0;

        virtual void add_clause(int l1, int l2, int l3)=0;

        virtual void add_clause(int l1, int l2, int l3, int l4)=0;

        virtual int solve(const std::vector<int>* assumptions, double time_limit)=0;

        int solve() { return solve(nullptr, NO_TIME_LIMIT); }

        int solve(const std::vector<int>* assumptions) { return solve(assumptions, NO_TIME_LIMIT); }

        int solve(const double time_limit) { return solve(nullptr, time_limit); }

        [[nodiscard]] virtual std::unordered_map<std::string, double> get_statistics() const;

        virtual void reset()=0;

        void encode_equals_k(const std::vector<int>& vars, int k);
    };
} // namespace SATSolver

#endif // BCP_SATSOLVER_H

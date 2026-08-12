//
// Created by homulily85 on 11/29/25.
//

#ifndef BCP_BMCP_UTILITY_H
#define BCP_BMCP_UTILITY_H

#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace BCPSolver
{
    enum SolverStatus
    {
        UNKNOWN = -1,
        UNSATISFIABLE = 0,
        SATISFIABLE = 1,
        OPTIMAL = 2,
    };

    enum SolvingMethod
    {
        TwoVariablesGreater,
        TwoVariablesLess,
        OneVariableGreater,
        OneVariableLess,
        StaircaseWithAuxiliaryVarsNoCache,
        StaircaseWithAuxiliaryVarsWithCache,
        StaircaseWithoutAuxiliaryVars
    };

    static constexpr double NO_TIME_LIMIT = std::numeric_limits<double>::lowest();

    class Graph
    {
    private:
        std::vector<std::tuple<int, int, int>> edges_list{};
        std::vector<std::vector<int>> matrix{};
        int n{};

    public:
        explicit Graph(const int n)
            : edges_list(std::vector<std::tuple<int, int, int>>()),
              matrix(std::vector(n < 0 ? 0 : n, std::vector<int>(n < 0 ? 0 : n))),
              n(n)
        {
            if (n < 0)
            {
                throw std::invalid_argument("Graph size must be non-negative");
            }
        }

        void add_edge(int i, int j, int w);

        [[nodiscard]] const std::vector<std::tuple<int, int, int>>& get_edges() const;

        [[nodiscard]] int get_weight(int i, int j) const;

        [[nodiscard]] int get_number_of_nodes() const;

        [[nodiscard]] int get_number_of_edges() const;

        [[nodiscard]] int get_highest_degree_vertex() const;

        [[nodiscard]] int get_degree(int node) const;

        [[nodiscard]] std::vector<int> get_neighbors(const int node) const
        {
            if (node < 0 || node >= n)
            {
                throw std::out_of_range("Node index out of bounds");
            }
            std::vector<int> neighbors;
            for (int j = 0; j < n; j++)
            {
                if (matrix[node][j] != 0)
                {
                    neighbors.push_back(j);
                }
            }
            return neighbors;
        }
    };

    Graph* read_bcp_graph(const std::string& file_path);

    struct ProgramConfig
    {
        std::string filename;
        double time_limit;
        int upper_bound;
        bool find_optimal;
        bool incremental_mode;
        std::string variable_for_incremental;
        bool use_symmetry_breaking;
        bool use_pairwise;
        std::string width;
        SolvingMethod solving_method;
        // Constructor with defaults
        ProgramConfig()
            : time_limit(NO_TIME_LIMIT), upper_bound(-1), find_optimal(true), incremental_mode(false),
              use_symmetry_breaking(false), use_pairwise(false),
              solving_method(TwoVariablesGreater)
        {
        }
    };

    class ArgParser
    {
    public:
        static void printUsage(const char* programName);

        static ProgramConfig parse(int argc, char* argv[]);
    };
} // namespace BCPSolver

#endif // BCP_BMCP_UTILITY_H

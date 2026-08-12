#include "bcp_solver/bcp_solver.h"

#include <iostream>
#include <memory>
#include <string>

int main(int argc, char* argv[])
{
    try
    {
        const BCPSolver::ProgramConfig config = BCPSolver::ArgParser::parse(argc, argv);

        const std::unique_ptr<BCPSolver::Graph> g(BCPSolver::read_bcp_graph(config.filename));
        if (!g)
        {
            return 1;
        }
        const std::unique_ptr<BCPSolver::BCPSolver> s(
            BCPSolver::BCPSolver::create_solver(config.solving_method, g.get(), config.upper_bound,
                                                config.use_symmetry_breaking, config.use_pairwise, config.width));
        s->solve(config.time_limit, config.find_optimal, config.incremental_mode, config.variable_for_incremental);
        for (auto stats = s->get_statistics(); const auto& [fst, snd] : stats)
        {
            std::cout << fst << ": " << snd << '\n';
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n\n";
        BCPSolver::ArgParser::printUsage(argv[0]);
        return 1;
    }

    return 0;
}

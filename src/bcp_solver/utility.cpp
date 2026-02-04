//
// Created by homulily85 on 11/29/25.
//

#include "utility.h"

#include <fstream>
#include <iostream>
#include <sstream>

void BCPSolver::Graph::add_edge(const int i, const int j, const int w)
{
    edges_list.emplace_back(i, j, w);
    matrix[i][j] = w;
    matrix[j][i] = w;
}

const std::vector<std::tuple<int, int, int>> &BCPSolver::Graph::get_edges() const
{
    return edges_list;
}

int BCPSolver::Graph::get_weight(int i, int j) const
{
    return matrix[i][j];
}

int BCPSolver::Graph::get_number_of_nodes() const
{
    return n;
}

int BCPSolver::Graph::get_number_of_edges() const
{
    return static_cast<int>(edges_list.size());
}

int BCPSolver::Graph::get_highest_degree_vertex() const
{
    if (n == 0)
    {
        return -1;
    }

    std::vector degrees(n, 0);

    for (const auto &edge : edges_list)
    {
        degrees[std::get<0>(edge)]++;
        degrees[std::get<1>(edge)]++;
    }

    int max_degree = -1;
    int max_vertex = -1;

    for (int i = 0; i < n; ++i)
    {
        if (degrees[i] > max_degree)
        {
            max_degree = degrees[i];
            max_vertex = i;
        }
    }

    return max_vertex;
}

int BCPSolver::Graph::get_degree(const int node) const
{
    if (node < 0 || node >= n)
    {
        throw std::out_of_range("Node index out of bounds");
    }

    int degree = 0;
    for (const int weight : matrix[node])
    {
        if (weight != 0)
        {
            degree++;
        }
    }

    return degree;
}

BCPSolver::Graph *BCPSolver::read_bcp_graph(const std::string &file_path)
{
    std::ifstream file(file_path);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open file " << file_path << std::endl;
        return nullptr;
    }

    Graph *g = nullptr;
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        std::stringstream ss(line);
        char line_type;
        ss >> line_type;

        if (line_type == 'p')
        {
            std::string ignore;
            int num_nodes;
            ss >> ignore >> num_nodes;

            g = new Graph(num_nodes);
        }
        else if (line_type == 'e')
        {
            if (g == nullptr)
                continue;

            int u, v, w;
            ss >> u >> v >> w;

            if (u != v)
            {
                g->add_edge(u - 1, v - 1, w);
            }
        }
    }

    file.close();
    return g;
}

void BCPSolver::ArgParser::printUsage(const char *programName)
{
    std::cerr << "Usage: " << programName << " <filename> <method> [options]\n"
              << "Arguments:\n"
              << "  <filename>                      Path to the input file\n"
              << "  <method>                        Method for solving: '1G', '1L','2G', '2L', 'Xa', 'X'\n\n"
              << "Options:\n"
              << "  -t, --time_limit <int>          Set time limit\n"
              << "  -ub, --upper_bound <int>        Set preferred upper bound\n"
              << "  --no-optimal                    Disable finding optimal value\n"
              << "  --use-symmetry-breaking         Enable symmetry breaking\n"
              << "  -w , --width <vary|fixed>       Set width for encoding."
                 "Note: This flag must be set for 'X', 'Xa' method. \n"
              << "  -i, --incremental               Enable incremental mode. "
                 "Note: This flag requires '-v' to be set as well.\n"
              << "  -v  --variable-for-incremental  Variables used in incremental: 'x, 'y'. You must specify this when"
                 " using incremental mode, but it will be ignored otherwise.\n"
              << "  -h, --help                      Show this help message\n";
}

BCPSolver::ProgramConfig BCPSolver::ArgParser::parse(int argc, char *argv[])
{
    ProgramConfig config;
    bool filenameFound = false;
    bool methodFound = false;

    for (int i = 1; i < argc; ++i)
    {
        if (std::string arg = argv[i]; arg == "-h" || arg == "--help")
        {
            printUsage(argv[0]);
            exit(0);
        }

        else if (arg == "-t" || arg == "--time_limit")
        {
            if (i + 1 < argc)
            {
                try
                {
                    config.time_limit = std::stoi(argv[++i]);
                    if (config.time_limit < 0)
                        throw std::exception();
                }
                catch (...)
                {
                    throw std::invalid_argument("Invalid time limit: " + std::string(argv[i]));
                }
            }
            else
                throw std::invalid_argument("Missing value for time limit");
        }
        else if (arg == "-ub" || arg == "--upper_bound")
        {
            if (i + 1 < argc)
            {
                try
                {
                    config.upper_bound = std::stoi(argv[++i]);
                    if (config.upper_bound < 0)
                        throw std::exception();
                }
                catch (...)
                {
                    throw std::invalid_argument("Invalid upper_bound: " + std::string(argv[i]));
                }
            }
            else
                throw std::invalid_argument("Missing value for upper bound");
        }
        else if (arg == "--use-symmetry-breaking")
        {
            config.use_symmetry_breaking = true;
        }

        else if (arg == "-w" || arg == "--width")
        {
            if (i + 1 < argc)
            {
                if (std::string width = argv[++i]; width == "vary" || width == "fixed")
                {
                    config.width = width;
                }
                else
                {
                    throw std::invalid_argument("Invalid width: " + width + ". Expected 'vary' or 'fixed'.");
                }
            }
            else
                throw std::invalid_argument("Missing value for width");
        }
        else if (arg == "--no-optimal")
        {
            config.find_optimal = false;
        }
        else if (arg == "-i" || arg == "--incremental")
        {
            config.incremental_mode = true;
        }
        else if (arg == "-v" || arg == "--variable-for-incremental")
        {
            if (i + 1 < argc)
            {
                if (std::string var = argv[++i]; var == "x" || var == "y")
                {
                    config.variable_for_incremental = var;
                }
                else
                {
                    throw std::invalid_argument("Invalid variable for incremental: " + var + ". Expected 'x', 'y'.");
                }
            }
            else
                throw std::invalid_argument("Missing value for variable for incremental");
        }
        else if (arg[0] == '-')
        {
            throw std::invalid_argument("Unknown flag: " + arg);
        }
        else
        {
            // Logic for Positional Arguments
            if (!filenameFound)
            {
                config.filename = arg;
                filenameFound = true;
            }
            else if (!methodFound)
            {
                if (arg == "1G")
                {
                    config.solving_method = OneVariableGreater;
                }
                else if (arg == "2G")
                {
                    config.solving_method = TwoVariablesGreater;
                }
                else if (arg == "1L")
                {
                    config.solving_method = OneVariableLess;
                }
                else if (arg == "2L")
                {
                    config.solving_method = TwoVariablesLess;
                }
                else if (arg == "Xa")
                {
                    config.solving_method = StaircaseWithAuxiliaryVarsWithCache;
                }
                else if (arg == "X")
                {
                    config.solving_method = StaircaseWithoutAuxiliaryVars;
                }
                else
                {
                    throw std::invalid_argument("Invalid method: " + arg +
                                                ". Expected '1G', '1L','2G', '2L', 'Xa', 'X'.");
                }
                methodFound = true;
            }
            else
            {
                throw std::invalid_argument("Unexpected extra argument: " + arg);
            }
        }
    }

    if (!filenameFound)
        throw std::runtime_error("Missing compulsory argument: <filename>");
    if (!methodFound)
        throw std::runtime_error("Missing compulsory argument: <method>");

    return config;
}

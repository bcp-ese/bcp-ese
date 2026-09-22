//
// Created by homulily85 on 1/16/26.
//

#include "Kissat.h"
#include <cstdlib>

#include <filesystem>
#include <fstream>
#include <random>

#include "cadical.hpp"

namespace fs = std::filesystem;

std::string SATSolver::Kissat::get_random_filename()
{
    static constexpr char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    constexpr std::size_t len = 5;

    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<std::size_t> dist(0, sizeof(alphanum) - 2);

    std::string s;
    s.reserve(len);
    for (std::size_t i = 0; i < len; ++i)
    {
        s += alphanum[dist(rng)];
    }
    return s;
}

std::string SATSolver::Kissat::write_cnf_to_file() const
{
    const std::string filename = "cnf/" + get_random_filename() + ".cnf";

    if (fs::path folder = "cnf"; !fs::exists(folder))
    {
        if (!fs::create_directory(folder))
        {
            throw std::runtime_error("Failed to create directory");
        }
    }

    std::ofstream cnf_file(filename);
    cnf_file << "p cnf " << number_of_variables << " " << number_of_clauses << "\n";
    for (const auto& clause : clauses)
    {
        for (const int lit : clause)
        {
            cnf_file << lit << " ";
        }
        cnf_file << "0\n";
    }
    cnf_file.close();

    return filename;
}

void SATSolver::Kissat::add_clause(const std::vector<int>& clause)
{
    number_of_clauses++;
    clauses.push_back(clause);
}

void SATSolver::Kissat::add_clause(int l)
{
    number_of_clauses++;
    clauses.push_back({l});
}

void SATSolver::Kissat::add_clause(int l1, int l2)
{
    number_of_clauses++;
    if (l1 < l2)
    {
        clauses.push_back({l1, l2});
    }
    else
    {
        clauses.push_back({l2, l1});
    }
}

void SATSolver::Kissat::add_clause(int l1, int l2, int l3)
{
    number_of_clauses++;
    const int x = std::min({l1, l2, l3});
    const int z = std::max({l1, l2, l3});
    const int y = l1 + l2 + l3 - x - z;
    clauses.push_back({x, y, z});
}

void SATSolver::Kissat::add_clause(int l1, int l2, int l3, int l4)
{
    number_of_clauses++;
    clauses.push_back({l1, l2, l3, l4});
}

int SATSolver::Kissat::solve(const std::vector<int>* assumptions, const double time_limit)
{
    if (!std::filesystem::exists(KISSAT_PATH)) {
        throw std::runtime_error("kissat not found: " + std::string(KISSAT_PATH));
    }

    if (time_limit != NO_TIME_LIMIT && time_limit < 0.0)
    {
        status = CaDiCaL::Status::UNKNOWN;
        return status;
    }

    if (assumptions != nullptr)
    {
        throw std::runtime_error("Kissat solver does not support assumptions.");
    }

    const auto start_time = std::chrono::high_resolution_clock::now();

    const auto file_name = write_cnf_to_file();

    if (time_limit == NO_TIME_LIMIT)
    {
        const std::string cmd = std::string(KISSAT_PATH) + " " + file_name + " -q -n";
        const int status = system(cmd.c_str());

        if (const int exitCode = WEXITSTATUS(status); exitCode == 10)
        {
            this->status = CaDiCaL::Status::SATISFIABLE;
        }
        else if (exitCode == 20)
        {
            this->status = CaDiCaL::Status::UNSATISFIABLE;
        }
        else
        {
            this->status = CaDiCaL::Status::UNKNOWN;
        }
    }
    else
    {
        const std::string cmd =
            "timeout " + std::to_string(static_cast<int>(time_limit)) +
            " " + std::string(KISSAT_PATH) + " " + file_name + " -q -n";
        const int status = system(cmd.c_str());

        if (const int exitCode = WEXITSTATUS(status); exitCode == 10)
        {
            this->status = CaDiCaL::Status::SATISFIABLE;
        }
        else if (exitCode == 20)
        {
            this->status = CaDiCaL::Status::UNSATISFIABLE;
        }
        else
        {
            this->status = CaDiCaL::Status::UNKNOWN;
        }
    }

    time_accum += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start_time).count();

    return this->status;
}

void SATSolver::Kissat::reset()
{
    number_of_clauses = 0;
    number_of_variables = 0;
    clauses.clear();
}

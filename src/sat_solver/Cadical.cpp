//
// Created by homulily85 on 1/13/26.
//

#include "Cadical.h"

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#define DUP _dup
#define DUP2 _dup2
#define FILENO _fileno
#define GETPID _getpid
#define UNLINK _unlink
#else
#include <unistd.h>
#define DUP dup
#define DUP2 dup2
#define FILENO fileno
#define GETPID getpid
#define UNLINK unlink
#endif

// Static mutex to protect stdout redirection (process-global resource)
static std::mutex stdout_capture_mutex;

SATSolver::Cadical::Cadical()
{
    if (!solver->set("seed", SOLVER_SEED))
    {
        throw std::runtime_error("CaDiCaL does not expose the expected seed option.");
    }
}

void SATSolver::Cadical::capture_and_accumulate_stats()
{
    std::lock_guard<std::mutex> lock(stdout_capture_mutex);

    const auto temp_path = std::filesystem::temp_directory_path() /
        ("bcp-cadical-stats-" + std::to_string(GETPID()) + ".tmp");
    const std::string temp_filename = temp_path.string();

    // 1. Flush current stdout to ensure clean state
    fflush(stdout);

    // 2. Save the current stdout file descriptor
    int original_stdout_fd = DUP(FILENO(stdout));

    // 3. Redirect stdout to our temp file
    if (freopen(temp_filename.c_str(), "w", stdout) == nullptr)
    {
        // Fallback: If redirection fails, restore and exit
        // (Original fd is still open, just close duplicate)
#ifdef _WIN32
        _close(original_stdout_fd);
#else
        close(original_stdout_fd);
#endif
        return;
    }

    // 4. Call CaDiCaL statistics (writes to the temp file now)
    solver->statistics();

    // 5. Flush and Restore stdout
    fflush(stdout);
    DUP2(original_stdout_fd, FILENO(stdout)); // Replace stdout with original

    // Close the backup fd
#ifdef _WIN32
    _close(original_stdout_fd);
#else
    close(original_stdout_fd);
#endif

    // 6. Read the temp file and parse
    std::ifstream infile(temp_filename);
    std::unordered_map<std::string, double> current_snapshot;
    std::string line;
    while (std::getline(infile, line))
    {
        // Skip lines that don't look like stats
        if (line.empty() || line[0] != 'c') continue;

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        while (ss >> token)
        {
            tokens.push_back(token);
        }

        // Check format: "c [key]: [value] ..."
        // Need at least "c", "key:", "value"
        if (tokens.size() < 3) continue;

        // Iterate to find the key ending in ':'
        for (size_t i = 1; i < tokens.size() - 1; ++i)
        {
            if (tokens[i].back() == ':')
            {
                std::string key = tokens[i];
                key.pop_back(); // Remove ':'

                try
                {
                    // The next token is the value
                    double value = std::stod(tokens[i + 1]);

                    current_snapshot[key] = value;
                }
                catch (...)
                {
                    // Ignore non-numeric values
                }
                break; // Found the stat for this line, move to next line
            }
        }
    }
    infile.close();

    for (const auto& [key, value] : current_snapshot)
    {
        const auto previous = last_stats_snapshot.find(key);
        const double delta = previous == last_stats_snapshot.end() || value < previous->second
                                 ? value
                                 : value - previous->second;
        stats_accum[key] += delta;
    }
    last_stats_snapshot = std::move(current_snapshot);

    // 7. Delete the temp file
    UNLINK(temp_filename.c_str());
}

void SATSolver::Cadical::add_clause(const std::vector<int>& clause)
{
    number_of_clauses++;
    solver->clause(clause);
}

void SATSolver::Cadical::add_clause(const int l)
{
    number_of_clauses++;
    solver->clause(l);
    // std::cout << '[' << l << ']' << '\n';
}

void SATSolver::Cadical::add_clause(const int l1, const int l2)
{
    number_of_clauses++;
    if (l1 < l2)
    {
        solver->clause(l1, l2);
    }

    else
    {
        solver->clause(l2, l1);
    }
    // std::cout << '[' << l1 << ", " << l2 << ']' << '\n';
}

void SATSolver::Cadical::add_clause(const int l1, const int l2, const int l3)
{
    number_of_clauses++;
    const int x = std::min({l1, l2, l3});
    const int z = std::max({l1, l2, l3});
    const int y = l1 + l2 + l3 - x - z;
    solver->clause(x, y, z);
    // std::cout << '[' << l1 << ", " << l2 << ", " << l3 << ']' << '\n';
}

void SATSolver::Cadical::add_clause(const int l1, const int l2, const int l3, const int l4)
{
    number_of_clauses++;
    solver->clause(l1, l2, l3, l4);
    // std::cout << '[' << l1 << ", " << l2 << ", " << l3 << ", " << l4 << ']' << '\n';
}

int SATSolver::Cadical::solve(const std::vector<int>* assumptions, const double time_limit)
{
    if (time_limit != NO_TIME_LIMIT && time_limit < 0.0)
    {
        status = CaDiCaL::Status::UNKNOWN;
        return status;
    }

    if (assumptions != nullptr)
    {
        for (const auto assumption : *assumptions)
        {
            solver->assume(assumption);
        }
    }

    const auto start_time = std::chrono::high_resolution_clock::now();

    if (time_limit == NO_TIME_LIMIT)
    {
        status = solver->solve();
        time_accum += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start_time).count();
        capture_and_accumulate_stats();
    }
    else
    {
        AtomicTerminator terminator;
        solver->connect_terminator(&terminator);

        std::mutex mtx;
        std::condition_variable cv;
        bool finished = false;

        std::thread timer_thread([&]()
        {
            std::unique_lock<std::mutex> lk(mtx);
            if (!cv.wait_for(lk, std::chrono::duration<double>(time_limit), [&] { return finished; }))
            {
                terminator.force_terminate.store(true, std::memory_order_relaxed);
            }
        });

        status = solver->solve();

        {
            std::lock_guard<std::mutex> lk(mtx);
            finished = true;
        }
        cv.notify_one();

        if (timer_thread.joinable())
        {
            timer_thread.join();
        }

        solver->disconnect_terminator();
        time_accum += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start_time).count();
        capture_and_accumulate_stats();
    }

    return status;
}

void SATSolver::Cadical::reset()
{
    number_of_clauses = 0;
    number_of_variables = 0;
    solver = std::make_unique<CaDiCaL::Solver>();
    if (!solver->set("seed", SOLVER_SEED))
    {
        throw std::runtime_error("CaDiCaL does not expose the expected seed option.");
    }
    last_stats_snapshot.clear();
}

std::unordered_map<std::string, double> SATSolver::Cadical::get_statistics() const
{
    auto final_stats = stats_accum;

    // Add/Overwrite with manual SATSolver base metrics
    final_stats["total_solving_time"] = time_accum;
    final_stats["clauses"] = number_of_clauses;
    final_stats["variables"] = number_of_variables;
    final_stats["solver_seed"] = SOLVER_SEED;

    return final_stats;
}

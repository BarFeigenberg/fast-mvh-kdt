#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>

#include <boost/program_options.hpp>

#include "fast_mvh/solvers/l_namoa_kdt_chooseh.h"
#include "fast_mvh/solvers/l_namoa_dr_mvh_kdt.h"
#include "parser.h"
#include "parsers/multi_valued_heuristic_parser.h"
#include "multivalued_heuristic/apex_mvh.h"

namespace po = boost::program_options;

int main(int argc, char** argv) {
    try {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "Produce help message")
            ("map,m", po::value<std::string>()->required(), "Directory for edge weights files")
            ("start,s", po::value<int>()->required(), "Start location")
            ("goal,g", po::value<int>()->required(), "Goal location")
            ("objectives", po::value<std::vector<int>>()->multitoken()->default_value(std::vector<int>{}, ""), "0-based indices of objectives to load")
            ("algorithm,a", po::value<std::string>()->default_value("L_NAMOA_KDT_CHOOSEH"), "Solver algorithm")
            ("mvh", po::value<std::string>()->default_value(""), "Directory for multi valued heuristic file")
            ("sol-out", po::value<std::string>()->default_value(""), "File to output solutions")
            ("stats-out", po::value<std::string>()->default_value(""), "File to output statistics")
            ("cutoffTime,t", po::value<unsigned int>()->default_value(120), "Cutoff time in seconds (0 = no limit)");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }

        po::notify(vm);

        std::string map_dir = vm["map"].as<std::string>();
        int start = vm["start"].as<int>();
        int goal = vm["goal"].as<int>();
        std::string algorithm = vm["algorithm"].as<std::string>();
        std::string mvh_path = vm["mvh"].as<std::string>();
        std::string sol_out = vm["sol-out"].as<std::string>();
        std::string stats_out = vm["stats-out"].as<std::string>();
        unsigned int timeout = vm["cutoffTime"].as<unsigned int>();

        std::vector<int> obj_raw = vm["objectives"].as<std::vector<int>>();
        std::vector<size_t> objectives(obj_raw.begin(), obj_raw.end());

        AdjacencyMatrix adj_matrix = Parser(map_dir, objectives).default_graph();

        MultiValuedHeuristic mvh = {};
        if (!mvh_path.empty()) {
            mvh = MVHParser::parse_heuristic(mvh_path);
        }

        EPS eps(adj_matrix.num_of_objectives, 0.0);
        if (mvh.empty()) {
            auto apex_mvh_builder = APEX_MVH(adj_matrix, eps);
            mvh = apex_mvh_builder(goal, "");
        }

        SolutionSet solutions;

        if (algorithm == "L_NAMOA_KDT_CHOOSEH" || algorithm == "L_NAMOA_DR_MVH_INSTRUMENTED") {
            L_NAMOA_KDT_CHOOSEH solver(adj_matrix, eps);
            solver.use_kdt_chooseh = (algorithm == "L_NAMOA_KDT_CHOOSEH");
            solver(start, goal, mvh, solutions, timeout, sol_out, stats_out);

            std::cout << "algorithm=" << algorithm
                      << "\tsource=" << start << "\ttarget=" << goal
                      << "\tnum_solutions=" << solutions.size()
                      << "\tnum_expansion=" << solver.num_expansion
                      << "\tnum_generation=" << solver.num_generation
                      << "\tnum_reinsertion=" << solver.num_reinsertion
                      << "\tnum_full_dominance_check=" << solver.num_full_dominance_check
                      << "\tnum_dr_dominance_check=" << solver.num_dr_dominance_check
                      << "\tnum_global_dominance_check=" << solver.num_global_dominance_check
                      << "\tcmp_chooseh=" << solver.cmp_chooseh
                      << "\truntime_s=" << solver.runtime / CLOCKS_PER_SEC
                      << "\ttime_limit_reached=" << solver.time_limit_reached
                      << "\n";
            solutions.clear();
        } else if (algorithm == "L_NAMOA_DR_MVH_KDT") {
            L_NAMOA_DR_MVH_KDT solver(adj_matrix, eps);
            solver(start, goal, mvh, solutions, timeout, sol_out, stats_out);

            std::cout << "algorithm=L_NAMOA_DR_MVH_KDT"
                      << "\tsource=" << start << "\ttarget=" << goal
                      << "\tnum_solutions=" << solutions.size()
                      << "\tnum_expansion=" << solver.num_expansion
                      << "\tnum_generation=" << solver.num_generation
                      << "\tnum_reinsertion=" << solver.num_reinsertion
                      << "\tnum_full_dominance_check=" << solver.num_full_dominance_check
                      << "\tnum_dr_dominance_check=" << solver.num_dr_dominance_check
                      << "\tnum_global_dominance_check=" << solver.num_global_dominance_check
                      << "\truntime_s=" << solver.runtime / CLOCKS_PER_SEC
                      << "\ttime_limit_reached=" << solver.time_limit_reached
                      << "\tcmpchk=" << solver.cmpchk
                      << "\tcmpupd=" << solver.cmpupd
                      << "\n";
            solutions.clear();
        } else {
            std::cerr << "error: unknown algorithm '" << algorithm << "'" << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

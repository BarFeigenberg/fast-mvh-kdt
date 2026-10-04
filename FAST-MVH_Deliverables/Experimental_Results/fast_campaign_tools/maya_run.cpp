// Thin driver around the unmodified reference L_NAMOA_DR_MVH (baselines/bridging-mvh-dr @ 0a2f9ea).
// It only adds a solutions-file argument, which the reference CLI does not expose.
// usage: maya_run MAP_DIR START GOAL MVH SOL_OUT [TIMEOUT_S]
#include <cstdlib>
#include <iostream>
#include <string>

#include "multivalued_heuristic/l_namoa_dr_mvh.h"
#include "parser.h"
#include "parsers/multi_valued_heuristic_parser.h"

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "usage: maya_run MAP_DIR START GOAL MVH SOL_OUT [TIMEOUT_S]\n";
        return 2;
    }
    const std::string map_dir = argv[1];
    const size_t start = std::strtoul(argv[2], nullptr, 10);
    const size_t goal = std::strtoul(argv[3], nullptr, 10);
    const std::string mvh_path = argv[4];
    const std::string sol_out = argv[5];
    const unsigned int timeout = argc > 6 ? static_cast<unsigned int>(std::strtoul(argv[6], nullptr, 10)) : 0;

    AdjacencyMatrix adj = Parser(map_dir, {}).default_graph();
    MultiValuedHeuristic mvh = MVHParser::parse_heuristic(mvh_path);
    const EPS eps(adj.num_of_objectives, 0.0);

    L_NAMOA_DR_MVH solver(adj, eps);
    SolutionSet solutions;
    solver(start, goal, mvh, solutions, timeout, sol_out, "");
    std::cout << "algorithm=L_NAMOA_DR_MVH"
              << "\tnum_solutions=" << solutions.size()
              << "\tnum_expansion=" << solver.num_expansion
              << "\tnum_generation=" << solver.num_generation
              << "\tnum_reinsertion=" << solver.num_reinsertion
              << "\tnum_full_dominance_check=" << solver.num_full_dominance_check
              << "\tnum_good_fallback=" << solver.num_good_fallback
              << "\tnum_bad_fallback=" << solver.num_bad_fallback
              << "\truntime_s=" << solver.runtime / CLOCKS_PER_SEC
              << "\ttime_limit_reached=" << solver.time_limit_reached << std::endl;
    return 0;
}

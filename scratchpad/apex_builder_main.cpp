#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "parser.h"
#include "multivalued_heuristic/apex_mvh.h"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char** argv) {
    std::string map_dir, out_file;
    int goal;
    std::vector<int> obj_raw;
    double epsilon;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("map", po::value<std::string>(&map_dir)->required(), "Map directory")
        ("goal", po::value<int>(&goal)->required(), "Goal node")
        ("objectives", po::value<std::vector<int>>(&obj_raw)->multitoken()->default_value(std::vector<int>{}, ""), "Objectives")
        ("eps", po::value<double>(&epsilon)->default_value(0.0), "Epsilon for APEX")
        ("out", po::value<std::string>(&out_file)->required(), "Output MVH file");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n" << desc << "\n";
        return 1;
    }

    std::vector<size_t> objectives(obj_raw.begin(), obj_raw.end());
    AdjacencyMatrix adj_matrix = Parser(map_dir, objectives).default_graph();
    EPS eps(adj_matrix.num_of_objectives, epsilon);

    std::cout << "Building A*pex MVH for goal " << goal << " with " << adj_matrix.num_of_objectives << " objectives...\n";
    APEX_MVH builder(adj_matrix, eps);
    MultiValuedHeuristic mvh = builder(goal, "");
    
    std::cout << "Writing to " << out_file << "...\n";
    std::ofstream out(out_file);
    if (!out) {
        std::cerr << "Failed to open " << out_file << "\n";
        return 1;
    }
    for (size_t s = 1; s < mvh.size(); ++s) {
        for (const auto& vec : mvh[s]) {
            out << s;
            for (size_t i = 0; i < vec.size(); ++i) {
                out << "\t" << vec[i];
            }
            out << "\n";
        }
    }
    std::cout << "Done.\n";
    return 0;
}

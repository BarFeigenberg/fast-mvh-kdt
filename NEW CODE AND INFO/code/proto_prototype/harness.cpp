// Scratch harness: runs Maya's baseline and the existing src/ integration solvers
// through one driver (no Boost.ProgramOptions dependency).
// usage: harness <map_dir> <start> <goal> <M> <mvh_file|apex|apex-dump:FILE> <algo> [sol_out]
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "parser.h"
#include "parsers/multi_valued_heuristic_parser.h"
#include "multivalued_heuristic/apex_mvh.h"
#include "multivalued_heuristic/l_namoa_dr_mvh.h"
#include "fast_mvh/solvers/l_namoa_kdt_chooseh.h"
#include "fast_mvh/solvers/l_namoa_dr_mvh_kdt.h"
#include "fast_mvh/solvers/l_namoa_dr_mvh_fast.h"
#define NOMINMAX
#include <windows.h>

static void dump_solutions(const SolutionSet& sols, const std::string& path) {
    if (path.empty()) return;
    std::ofstream out(path);
    for (const auto& s : sols) {
        for (size_t i = 0; i < s->g.size(); ++i) { if (i) out << ","; out << s->g[i]; }
        out << "\n";
    }
}

int main(int argc, char** argv) {
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    if (const char* a = std::getenv("PROTO_AFFINITY")) SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)std::stoull(a));
    if (argc < 7) { std::cerr << "usage\n"; return 2; }
    std::string map_dir = argv[1];
    size_t start = std::stoul(argv[2]), goal = std::stoul(argv[3]), M = std::stoul(argv[4]);
    std::string mvh_arg = argv[5], algo = argv[6];
    std::string sol_out = argc > 7 ? argv[7] : "";

    std::vector<size_t> objs; for (size_t i = 0; i < M; ++i) objs.push_back(i);
    AdjacencyMatrix adj = Parser(map_dir, objs).default_graph();
    EPS eps(adj.num_of_objectives, 0.0);
    MultiValuedHeuristic mvh;
    if (mvh_arg.rfind("apex", 0) == 0) {
        std::string dump = mvh_arg.size() > 10 ? mvh_arg.substr(10) : "";
        APEX_MVH b(adj, eps); mvh = b(goal, dump);
        if (algo == "NONE") return 0;
    } else {
        mvh = MVHParser::parse_heuristic(mvh_arg);
    }
    if (mvh.size() < adj.size() + 1) mvh.resize(adj.size() + 1);

    SolutionSet sols;
    double wall = 0; std::string extra;
    size_t nsol = 0, nexp = 0, ngen = 0, nre = 0, nfull = 0; uint64_t cmp = 0;
    auto finish = [&](auto& s, auto t0) {
        wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        dump_solutions(sols, sol_out);
        nsol = sols.size(); sols.clear();
        nexp = s.num_expansion; ngen = s.num_generation; nre = s.num_reinsertion; nfull = s.num_full_dominance_check;
    };
    if (algo == "MAYA") {
        L_NAMOA_DR_MVH s(adj, eps);
        auto t0 = std::chrono::steady_clock::now(); s(start, goal, mvh, sols, 0); finish(s, t0);
    } else if (algo == "FAST") {
        L_NAMOA_DR_MVH_FAST s(adj, eps);
        if (const char* v = std::getenv("FAST_HTREE")) s.heuristic_tree_min = std::stoull(v);
        if (const char* v = std::getenv("FAST_C")) s.promotion.mean_scan = std::stoull(v);
        if (const char* v = std::getenv("FAST_MIN")) s.promotion.min_size = std::stoull(v);
        auto t0 = std::chrono::steady_clock::now(); s(start, goal, mvh, sols, 0); finish(s, t0);
        cmp = s.cmpchk + s.cmpupd + s.cmp_chooseh + s.cmp_full;
        extra = "\tfull_checks=" + std::to_string(s.num_full_dominance_check) + "\tfrontier_trees=" + std::to_string(s.num_frontier_trees) +
                "\theuristic_trees=" + std::to_string(s.num_heuristic_trees) + "\tredundant_h=" + std::to_string(s.num_redundant_heuristics);
    } else if (algo == "SHAHAF") {
        L_NAMOA_DR_MVH_KDT s(adj, eps);
        auto t0 = std::chrono::steady_clock::now(); s(start, goal, mvh, sols, 0); finish(s, t0);
        cmp = s.cmpchk + s.cmpupd;
    } else {
        L_NAMOA_KDT_CHOOSEH s(adj, eps);
        s.use_kdt_chooseh = (algo != "INSTR");
        if (algo == "V1") s.variant = L_NAMOA_KDT_CHOOSEH::Variant::V1;
        else if (algo == "V2") s.variant = L_NAMOA_KDT_CHOOSEH::Variant::V2;
        else if (algo == "V3") s.variant = L_NAMOA_KDT_CHOOSEH::Variant::V3;
        else if (algo == "V5") s.variant = L_NAMOA_KDT_CHOOSEH::Variant::V5;
        auto t0 = std::chrono::steady_clock::now(); s(start, goal, mvh, sols, 0); finish(s, t0);
        cmp = s.cmp_chooseh;
    }
    std::printf("algo=%s\tsols=%zu\texp=%zu\tgen=%zu\treins=%zu\tfull=%zu\tcmp=%llu\twall=%.4f",
                algo.c_str(), nsol, nexp, ngen, nre, nfull, (unsigned long long)cmp, wall);
    std::printf("%s\n", extra.c_str());
    return 0;
}


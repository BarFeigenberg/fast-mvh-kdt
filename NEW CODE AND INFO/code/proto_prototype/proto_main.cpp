// usage: proto <map_dir> <start> <goal> <M> <mvh_file> <sol_out|-> [key=value ...]
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "parser.h"
#include "parsers/multi_valued_heuristic_parser.h"
#include "proto.hpp"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

using namespace proto;

static bool prof = false;
static size_t parse_sz(const std::string& v) { return v == "inf" ? INF : std::stoull(v); }

template <int D>
int run(const AdjacencyMatrix& adj, const MultiValuedHeuristic& mvh, size_t s, size_t g, const Cfg& cfg, const std::string& sol_out) {
    SolutionSet sols;
    double wall;
    Stats st;
    {
        Solver<D> S(adj, cfg); S.st.prof = prof;
        auto t0 = std::chrono::steady_clock::now();
        S.run(s, g, mvh, sols);
        wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        st = S.st;
        if (sol_out != "-") {
            std::ofstream out(sol_out);
            for (const auto& n : sols) { for (size_t i = 0; i < n->g.size(); ++i) { if (i) out << ","; out << n->g[i]; } out << "\n"; }
        }
        size_t ns = sols.size(); sols.clear();
        double tt = (double)st.t_total;
        auto pct = [&](uint64_t x) { return 100.0 * (double)x / tt; };
        std::printf("sols=%zu\texp=%zu\tgen=%zu\treins=%zu\twall=%.4f", ns, st.expansions, st.generations, st.reinsertions, wall);
        std::printf("\tp_global=%.1f\tp_ch_gen=%.1f\tp_ch_re=%.1f\tp_local=%.1f\tp_full=%.1f\tp_update=%.1f\tp_build=%.1f",
                    pct(st.t_global), pct(st.t_ch_gen), pct(st.t_ch_re), pct(st.t_local), pct(st.t_full), pct(st.t_update), pct(st.t_build));
        std::printf("\tcmpchk=%llu\tcmpupd=%llu\tcmpfull=%llu\tcmpch=%llu", (unsigned long long)st.cmpchk, (unsigned long long)st.cmpupd,
                    (unsigned long long)st.cmpfull, (unsigned long long)st.cmp_ch);
        std::printf("\tch_gen=%llu\tch_re=%llu\tch_first=%llu\tch_later=%llu\tch_none=%llu\tch_ideal=%llu\tch_tree=%llu\tch_nodes=%llu\tavg_skip=%.2f\tavg_H=%.1f\tavg_T=%.1f",
                    (unsigned long long)st.ch_calls_gen, (unsigned long long)st.ch_calls_re, (unsigned long long)st.ch_first,
                    (unsigned long long)st.ch_later, (unsigned long long)st.ch_none, (unsigned long long)st.ch_ideal_prune,
                    (unsigned long long)st.ch_tree_calls, (unsigned long long)st.ch_nodes,
                    (double)st.ch_skip_sum / std::max<uint64_t>(1, st.ch_first + st.ch_later),
                    (double)st.ch_H_sum / std::max<uint64_t>(1, st.ch_calls_gen + st.ch_calls_re),
                    (double)st.ch_T_sum / std::max<uint64_t>(1, st.ch_calls_gen + st.ch_calls_re));
        std::printf("\tloc=%llu\tloc_notr=%llu\tloc_tdisc=%llu\tloc_bad=%llu\tloc_good=%llu\tkd_builds=%llu\tkd_rebuilds=%llu\thtrees=%llu",
                    (unsigned long long)st.loc_calls, (unsigned long long)st.loc_notr, (unsigned long long)st.loc_tdisc,
                    (unsigned long long)st.loc_bad, (unsigned long long)st.loc_good, (unsigned long long)st.kd_builds,
                    (unsigned long long)st.kd_rebuilds, (unsigned long long)st.htrees);
        std::printf("\tdomT=%llu", (unsigned long long)st.domT_calls);
        std::printf("\th_redund=%llu\th_total=%llu\tch_rskip=%llu", (unsigned long long)st.h_redundant, (unsigned long long)st.h_total, (unsigned long long)st.ch_rskip);
        std::printf("\tloc_hist=");
        for (int i = 0; i < 24; ++i) std::printf("%llu%s", (unsigned long long)st.loc_size_hist[i], i < 23 ? "," : "");
        std::printf("\tT_hist=");
        for (int i = 0; i < 24; ++i) std::printf("%llu%s", (unsigned long long)st.T_size_hist[i], i < 23 ? "," : "");
        auto dump = [&](const char* nm, const uint64_t* cy, const uint64_t* cn) {
            std::printf("\t%s=", nm);
            for (int i = 0; i < 24; ++i) std::printf("%.0f/%llu%s", cn[i] ? (double)cy[i] / cn[i] : 0.0, (unsigned long long)cn[i], i < 23 ? "," : "");
        };
        if (prof) { dump("loc_cpq", st.loc_cyc, st.loc_cnt); dump("T_cpq", st.T_cyc, st.T_cnt); dump("upd_cpo", st.upd_cyc, st.upd_cnt); }
        std::printf("\n");
    }
    return 0;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    if (const char* a = std::getenv("PROTO_AFFINITY")) SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)std::stoull(a));
#endif
    if (argc < 7) { std::cerr << "usage\n"; return 2; }
    std::string map_dir = argv[1];
    size_t s = std::stoul(argv[2]), g = std::stoul(argv[3]), M = std::stoul(argv[4]);
    std::string mvh_path = argv[5], sol_out = argv[6];
    Cfg cfg;
    for (int i = 7; i < argc; ++i) {
        std::string a = argv[i]; auto p = a.find('=');
        std::string k = a.substr(0, p), v = a.substr(p + 1);
        if (k == "localX") cfg.localX = parse_sz(v);
        else if (k == "targetX") cfg.targetX = parse_sz(v);
        else if (k == "chooseh") cfg.chooseh = std::stoi(v);
        else if (k == "hX") cfg.hX = parse_sz(v);
        else if (k == "leafB") cfg.leafB = parse_sz(v);
        else if (k == "fastpath") cfg.fastpath = v == "1";
        else if (k == "ideal") cfg.suffix_ideal = v == "1";
        else if (k == "local_first") cfg.local_first = v == "1";
        else if (k == "filterT") cfg.filterT = v == "1";
        else if (k == "gcl_prune") cfg.gcl_prune = v == "1";
        else if (k == "exact_max") cfg.exact_max = v == "1";
        else if (k == "branchless") cfg.branchless = v == "1";
        else if (k == "flatH") cfg.flatH = v == "1";
        else if (k == "witness") cfg.witness = v == "1";
        else if (k == "redund") cfg.redund = v == "1";
        else if (k == "lastwit") cfg.lastwit = v == "1";
        else if (k == "rb_div") cfg.rb_div = parse_sz(v);
        else if (k == "rb_slack") cfg.rb_slack = parse_sz(v);
        else if (k == "bk") cfg.bk_cap = (uint32_t)parse_sz(v);
        else if (k == "promoteC") cfg.promoteC = parse_sz(v);
        else if (k == "prof") prof = v == "1";
        else { std::cerr << "unknown key " << k << "\n"; return 2; }
    }
    std::vector<size_t> objs; for (size_t i = 0; i < M; ++i) objs.push_back(i);
    AdjacencyMatrix adj = Parser(map_dir, objs).default_graph();
    MultiValuedHeuristic mvh = MVHParser::parse_heuristic(mvh_path);
    if (mvh.size() < adj.size() + 1) mvh.resize(adj.size() + 1);
    switch (M) {
        case 3: return run<3>(adj, mvh, s, g, cfg, sol_out);
        case 4: return run<4>(adj, mvh, s, g, cfg, sol_out);
        case 5: return run<5>(adj, mvh, s, g, cfg, sol_out);
        case 6: return run<6>(adj, mvh, s, g, cfg, sol_out);
        case 7: return run<7>(adj, mvh, s, g, cfg, sol_out);
        case 8: return run<8>(adj, mvh, s, g, cfg, sol_out);
    }
    return 3;
}






#pragma once
#include "data_structures/adjacency_matrix.h"
#include <vector>
#include <cmath>
#include <numeric>

inline size_t compute_optimal_dr_coordinate(const AdjacencyMatrix& adj) {
    size_t M = adj.num_of_objectives;
    if (M <= 1) return 0;
    
    std::vector<std::vector<double>> costs;
    for (size_t u = 1; u < adj.size(); ++u) {
        for (const auto& edge : adj[u]) {
            std::vector<double> c(M);
            for (size_t d = 0; d < M; ++d) c[d] = (double)edge.cost[d];
            costs.push_back(c);
        }
    }
    size_t N = costs.size();
    if (N == 0) return 0;
    
    std::vector<double> means(M, 0.0);
    for (const auto& c : costs) {
        for (size_t d = 0; d < M; ++d) means[d] += c[d];
    }
    for (size_t d = 0; d < M; ++d) means[d] /= N;
    
    std::vector<double> vars(M, 0.0);
    std::vector<std::vector<double>> cov(M, std::vector<double>(M, 0.0));
    
    for (const auto& c : costs) {
        for (size_t i = 0; i < M; ++i) {
            for (size_t j = 0; j < M; ++j) {
                cov[i][j] += (c[i] - means[i]) * (c[j] - means[j]);
            }
        }
    }
    
    for (size_t i = 0; i < M; ++i) {
        for (size_t j = 0; j < M; ++j) {
            cov[i][j] /= N;
        }
        vars[i] = cov[i][i];
    }
    
    size_t best_k = 0;
    double max_score = -1e9;
    
    for (size_t i = 0; i < M; ++i) {
        double score = vars[i];
        for (size_t j = 0; j < M; ++j) {
            if (i == j) continue;
            double std_i = std::sqrt(vars[i]);
            double std_j = std::sqrt(vars[j]);
            if (std_i > 1e-9 && std_j > 1e-9) {
                double rho = cov[i][j] / (std_i * std_j);
                score += rho;
            }
        }
        if (score > max_score) {
            max_score = score;
            best_k = i;
        }
    }
    return best_k;
}

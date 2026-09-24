import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

CSV_PATH = "benchmarks/runs/adaptive_explorer/v5_breakthrough_scaling.csv"
OUT_DIR = "benchmarks/runs/adaptive_explorer"

def generate_plots():
    if not os.path.exists(CSV_PATH):
        print(f"File not found: {CSV_PATH}")
        return
        
    df = pd.read_csv(CSV_PATH)
    
    # Filter valid numerical runtimes
    def clean_col(s):
        return pd.to_numeric(s, errors='coerce')
        
    df['V3_Time_s'] = clean_col(df['V3_Time_s'])
    df['V5_Time_s'] = clean_col(df['V5_Time_s'])
    df['Maya_Time_s'] = clean_col(df['Maya_Time_s'])
    df['K'] = pd.to_numeric(df['K'], errors='coerce')
    df['M'] = pd.to_numeric(df['M'], errors='coerce')
    df['N'] = pd.to_numeric(df['N'], errors='coerce')
    
    # Compute true numerical speedups where both finished
    df['V3_Speedup_Num'] = df['Maya_Time_s'] / df['V3_Time_s']
    df['V5_Speedup_Num'] = df['Maya_Time_s'] / df['V5_Time_s']
    
    # -------------------------------------------------------------
    # Plot 1: Speedup vs K
    # -------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    
    # Subset 1: N=15, M=3, Rho=-0.6
    sub1 = df[(df['N'] == 15) & (df['M'] == 3) & (df['Rho'] == -0.6)].dropna(subset=['V3_Speedup_Num']).sort_values('K')
    if not sub1.empty:
        plt.plot(sub1['K'], sub1['V3_Speedup_Num'], 'o-', label='V3 (N=15, M=3, ρ=-0.6)', linewidth=2)
        plt.plot(sub1['K'], sub1['V5_Speedup_Num'], 's--', label='V5 (N=15, M=3, ρ=-0.6)', linewidth=2)
        
    # Subset 2: N=10, M=4
    sub2 = df[(df['N'] == 10) & (df['M'] == 4)].dropna(subset=['V3_Speedup_Num']).groupby('K').mean(numeric_only=True).reset_index().sort_values('K')
    if not sub2.empty:
        plt.plot(sub2['K'], sub2['V3_Speedup_Num'], '^-', label='V3 Mean (N=10, M=4)', linewidth=2)
        if 'V5_Speedup_Num' in sub2 and not sub2['V5_Speedup_Num'].isna().all():
            plt.plot(sub2['K'], sub2['V5_Speedup_Num'], 'd--', label='V5 Mean (N=10, M=4)', linewidth=2)

    # Subset 3: N=20, M=3 (Breakthrough points)
    sub3 = df[(df['N'] == 20) & (df['M'] == 3)].dropna(subset=['V5_Speedup_Num']).sort_values('K')
    if not sub3.empty:
        plt.plot(sub3['K'], sub3['V5_Speedup_Num'], 'P-', color='crimson', markersize=10, label='V5 Breakthrough (N=20, M=3)', linewidth=2.5)

    plt.axhline(1.0, color='gray', linestyle=':', label='Baseline parity (1.0x)')
    plt.xlabel('Heuristic Set Size K', fontsize=12)
    plt.ylabel('Empirical Speedup (Maya / KDT Method)', fontsize=12)
    plt.title('Speedup Factor vs. Multi-Valued Heuristic Count (K)', fontsize=14, fontweight='bold')
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend(loc='best', fontsize=10)
    plt.tight_layout()
    plot_k_path = os.path.join(OUT_DIR, "speedup_vs_K.png")
    plt.savefig(plot_k_path, dpi=300)
    plt.close()
    print(f"Saved: {plot_k_path}")

    # -------------------------------------------------------------
    # Plot 2: Speedup vs Dimension M
    # -------------------------------------------------------------
    plt.figure(figsize=(10, 6))
    
    # Group by Dimension M across comparable configurations
    valid = df.dropna(subset=['Maya_Time_s', 'V3_Time_s']).copy()
    if not valid.empty:
        m_grouped = valid.groupby('M').agg({
            'V3_Speedup_Num': ['mean', 'min', 'max'],
            'V5_Speedup_Num': ['mean', 'min', 'max']
        }).reset_index()
        
        m_vals = m_grouped['M'].values
        v3_means = m_grouped['V3_Speedup_Num']['mean'].values
        v5_means = m_grouped['V5_Speedup_Num']['mean'].values
        
        bar_width = 0.35
        x = np.arange(len(m_vals))
        
        plt.bar(x - bar_width/2, v3_means, bar_width, label='V3 (Expand-Only) Mean Speedup', color='#1f77b4', alpha=0.85)
        plt.bar(x + bar_width/2, v5_means, bar_width, label='V5 (Dual-Tree) Mean Speedup', color='#2ca02c', alpha=0.85)
        
        plt.axhline(1.0, color='crimson', linestyle='--', linewidth=1.5, label='Baseline parity (1.0x)')
        plt.xlabel('Objective Dimension (M)', fontsize=12)
        plt.ylabel('Mean Empirical Speedup vs Maya', fontsize=12)
        plt.title('Pruning Scaling vs. Objective Dimension (M)', fontsize=14, fontweight='bold')
        plt.xticks(x, [f"M={int(m)}" for m in m_vals], fontsize=11)
        plt.grid(True, linestyle='--', alpha=0.5, axis='y')
        plt.legend(loc='upper right', fontsize=10)
        plt.tight_layout()
        plot_dim_path = os.path.join(OUT_DIR, "speedup_vs_dimension.png")
        plt.savefig(plot_dim_path, dpi=300)
        plt.close()
        print(f"Saved: {plot_dim_path}")

if __name__ == "__main__":
    generate_plots()

import pandas as pd
import matplotlib.pyplot as plt
import os

csv_file = 'benchmarks/runs/adaptive_explorer/v5_breakthrough_scaling.csv'
out_dir = 'benchmarks/runs/adaptive_explorer/'

if os.path.exists(csv_file):
    df = pd.read_csv(csv_file)
    df = df[df['Maya_Time_s'] != 'TIMEOUT']
    df['Maya_Time_s'] = pd.to_numeric(df['Maya_Time_s'])
    
    # We want to plot Speedup vs K for a fixed N, M, Rho
    # Since we are varying K right now for N=15, M=3, Rho=-0.6
    subset = df[(df['N'] == 15) & (df['M'] == 3) & (df['Rho'] == -0.6)].sort_values(by='K')
    
    if not subset.empty:
        plt.figure(figsize=(10, 6))
        plt.plot(subset['K'], subset['Maya_Time_s'] / subset['V3_Time_s'], marker='o', label='V3 Speedup (Maya / V3)')
        plt.plot(subset['K'], subset['Maya_Time_s'] / subset['V5_Time_s'], marker='s', label='V5 Speedup (Maya / V5)')
        
        plt.axhline(1.0, color='r', linestyle='--', label='Baseline (1.0x)')
        plt.title('Speedup vs Target Frontier Size (K) [N=15, M=3, Rho=-0.6]')
        plt.xlabel('Target Frontier Size K (Number of States)')
        plt.ylabel('Speedup Factor (Higher is Better)')
        plt.legend()
        plt.grid(True)
        plt.tight_layout()
        plt.savefig(os.path.join(out_dir, 'speedup_vs_K.png'))
        print(f"Plot saved to {os.path.join(out_dir, 'speedup_vs_K.png')}")
else:
    print(f"CSV file {csv_file} not found.")

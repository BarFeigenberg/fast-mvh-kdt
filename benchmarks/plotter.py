import os
import pandas as pd
import matplotlib.pyplot as plt

def generate_plots():
    csv_path = "benchmarks/runs/adaptive_explorer/v5_breakthrough_scaling.csv"
    if not os.path.exists(csv_path):
        print("No CSV found yet.")
        return

    try:
        df = pd.read_csv(csv_path)
    except Exception as e:
        print(f"Error reading CSV: {e}")
        return

    # Convert > speedups (e.g., ">5.0x") to float for plotting
    for col in ['V3_Speedup', 'V5_Speedup']:
        df[col] = df[col].astype(str).str.replace('>', '').str.replace('x', '')
        df[col] = pd.to_numeric(df[col], errors='coerce')

    df_clean = df.dropna(subset=['V3_Speedup', 'V5_Speedup'])

    if df_clean.empty:
        print("No numeric data to plot yet.")
        return

    # 1. Speedup vs K
    plt.figure(figsize=(10, 6))
    for (n, rho), group in df_clean.groupby(['N', 'Rho']):
        plt.plot(group['K'], group['V3_Speedup'], marker='o', linestyle='--', label=f'V3 N={n}, rho={rho}')
        plt.plot(group['K'], group['V5_Speedup'], marker='x', linestyle='-', label=f'V5 N={n}, rho={rho}')
    plt.axhline(y=1.0, color='r', linestyle='--', label='Baseline (Maya)')
    plt.title('V3 Speedup vs Target Frontier Density (K)')
    plt.xlabel('K (Number of Heuristics)')
    plt.ylabel('Speedup (Maya Time / V3 Time)')
    plt.legend()
    plt.grid(True)
    plt.savefig('benchmarks/runs/adaptive_explorer/speedup_vs_K.png')
    plt.close()

if __name__ == "__main__":
    generate_plots()

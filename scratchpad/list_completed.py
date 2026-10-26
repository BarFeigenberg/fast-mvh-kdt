import csv

def summarize(path):
    print(f"=== {path} ===")
    try:
        with open(path, "r", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            count = 0
            for r in reader:
                count += 1
                n = r.get("N", "?")
                m = r.get("M", "?")
                rho = r.get("rho", "?")
                k = r.get("K", "?")
                tf = r.get("time_fast_s", "?")
                tm = r.get("time_maya_s", "?")
                sols = r.get("solutions", "?")
                sound = r.get("soundness", "?")
                sp = r.get("speedup_vs_maya", "?")
                print(f"[{count:02d}] N={n} M={m} rho={rho} K={k} | FAST: {tf}s | Maya: {tm}s | Speedup: {sp}x | Sols: {sols} | Soundness: {sound}")
    except Exception as e:
        print(f"Error reading {path}: {e}")

if __name__ == "__main__":
    summarize("benchmarks/runs/fast_scaling_up_to_8D.csv")
    summarize("benchmarks/runs/fast_scaling_overnight.csv")

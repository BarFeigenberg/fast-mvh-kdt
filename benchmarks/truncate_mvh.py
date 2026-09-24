import os
import sys

def limit_mvh_file(input_path, output_path, K):
    counts = {}
    with open(input_path, 'r') as fin, open(output_path, 'w') as fout:
        for line in fin:
            if not line.strip(): continue
            parts = line.strip().split()
            if not parts: continue
            
            node_id = parts[0]
            count = counts.get(node_id, 0)
            if count < K:
                fout.write(line)
                counts[node_id] = count + 1

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: py truncate_mvh.py <input.mvh> <output.mvh> <K>")
        sys.exit(1)
        
    in_file = sys.argv[1]
    out_file = sys.argv[2]
    K = int(sys.argv[3])
    
    print(f"Truncating {in_file} to max {K} vectors per state -> {out_file}")
    limit_mvh_file(in_file, out_file, K)
    print("Done.")

import os

def run():
    with open('benchmarks/adaptive_explorer.py', 'r') as f:
        code = f.read()

    # 1. Add V3 to CSV Header
    code = code.replace(
        '"Roi_Speedup_vs_Maya", "Roi_Cmp", "Maya_Cmp", "Cmp_Reduction"',
        '"Roi_Speedup_vs_Maya", "V3_Speedup_vs_Maya", "Roi_Cmp", "Maya_Cmp", "V3_Cmp"'
    )
    
    # 2. Add V3 to MD Header
    code = code.replace(
        '| N | M | Rho | K | Sols | Roi Time | Maya Time | Shahaf Time | Speedup (Maya/Roi) | Cmp Reduction |',
        '| N | M | Rho | K | Sols | V3 Time | Maya Time | Roi Time | Shahaf Time | V3 Speedup |'
    )
    code = code.replace(
        '|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|',
        '|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|'
    )
    code = code.replace(
        '''            rt = f"{r['Roi_Time_s']:.2f}" if isinstance(r['Roi_Time_s'], float) else r['Roi_Time_s']
            mt = f"{r['Maya_Time_s']:.2f}" if isinstance(r['Maya_Time_s'], float) else r['Maya_Time_s']
            st = f"{r['Shahaf_Time_s']:.2f}" if isinstance(r['Shahaf_Time_s'], float) else r['Shahaf_Time_s']
            su = f"**{r['Speedup']:.2f}x**" if isinstance(r['Speedup'], float) else r['Speedup']
            cr = f"**{r['Cmp_Reduction']:.2f}x**" if isinstance(r['Cmp_Reduction'], float) else r['Cmp_Reduction']
            f.write(f"| {r['N']}x{r['N']} | {r['M']} | {r['Rho']} | {r['K']} | {r['Sols']} | {rt} | {mt} | {st} | {su} | {cr} |\\n")''',
        '''            v3t = f"{r.get('V3_Time_s', '-'):.2f}" if isinstance(r.get('V3_Time_s'), float) else r.get('V3_Time_s', '-')
            mt = f"{r.get('Maya_Time_s', '-'):.2f}" if isinstance(r.get('Maya_Time_s'), float) else r.get('Maya_Time_s', '-')
            rt = f"{r.get('Roi_Time_s', '-'):.2f}" if isinstance(r.get('Roi_Time_s'), float) else r.get('Roi_Time_s', '-')
            st = f"{r.get('Shahaf_Time_s', '-'):.2f}" if isinstance(r.get('Shahaf_Time_s'), float) else r.get('Shahaf_Time_s', '-')
            su = f"**{r.get('V3_Speedup', '-'):.2f}x**" if isinstance(r.get('V3_Speedup'), float) else r.get('V3_Speedup', '-')
            f.write(f"| {r['N']}x{r['N']} | {r['M']} | {r['Rho']} | {r['K']} | {r['Sols']} | {v3t} | {mt} | {rt} | {st} | {su} |\\n")'''
    )
    
    # 3. Add V3 execution block inside main loop
    run_roi_block = '''        print(f" -> Roi (Timeout {ROI_TIMEOUT}s)...", end="", flush=True)
        roi_met = run_solver(inst_dir, mvh_path, N, M, "L_NAMOA_KDT_CHOOSEH", ROI_TIMEOUT)'''
        
    v3_block = '''        print(f" -> V3_ExpandOnly...", end="", flush=True)
        v3_met = run_solver(inst_dir, mvh_path, N, M, "L_NAMOA_KDT_V3", 600)
        
        print(f" -> Roi (Timeout {ROI_TIMEOUT}s)...", end="", flush=True)
        roi_met = run_solver(inst_dir, mvh_path, N, M, "L_NAMOA_KDT_CHOOSEH", ROI_TIMEOUT)'''
    
    code = code.replace(run_roi_block, v3_block)
    
    # 4. Handle V3 printing logic
    success_block = '''        if roi_met.get("runtime_s"):
            print(f" SUCCESS ({roi_met['runtime_s']:.2f}s)")
        else:
            print(f" TIMEOUT")'''
            
    new_success_block = '''        if v3_met.get("runtime_s"):
            print(f" SUCCESS ({v3_met['runtime_s']:.2f}s)")
        else:
            print(f" TIMEOUT")
            
        if roi_met.get("runtime_s"):
            print(f" -> Roi... SUCCESS ({roi_met['runtime_s']:.2f}s)")
        else:
            print(f" -> Roi... TIMEOUT")'''
            
    code = code.replace(success_block, new_success_block)
    
    # 5. Extract logic updates
    dict_update = '''        row_dict = {
            "N": N, "M": M, "Rho": rho, "K": K,
            "Sols": roi_met.get("num_solutions", "-"),
            "Generations": roi_met.get("num_generation", "-"),
            "Roi_Time_s": roi_met.get("runtime_s", "-"),
            "Maya_Time_s": maya_met.get("runtime_s", "-"),
            "Shahaf_Time_s": shahaf_met.get("runtime_s", "-"),
            "Speedup": (maya_met.get("runtime_s") / roi_met.get("runtime_s")) if maya_met.get("runtime_s") and roi_met.get("runtime_s") else "-",
            "Roi_Cmp": roi_met.get("cmp_chooseh", "-"),
            "Maya_Cmp": maya_met.get("cmp_chooseh", "-"),
            "Cmp_Reduction": (maya_met.get("cmp_chooseh") / roi_met.get("cmp_chooseh")) if maya_met.get("cmp_chooseh") and roi_met.get("cmp_chooseh") else "-"
        }
        
        results.append(row_dict)
        update_md(results)
        
        append_csv([
            row_dict["N"], row_dict["M"], row_dict["Rho"], row_dict["K"],
            row_dict["Sols"], row_dict["Generations"],
            row_dict["Roi_Time_s"], row_dict["Maya_Time_s"], row_dict["Shahaf_Time_s"],
            row_dict["Speedup"], row_dict["Roi_Cmp"], row_dict["Maya_Cmp"], row_dict["Cmp_Reduction"]
        ])'''
        
    new_dict_update = '''        row_dict = {
            "N": N, "M": M, "Rho": rho, "K": K,
            "Sols": v3_met.get("num_solutions", roi_met.get("num_solutions", "-")),
            "Generations": v3_met.get("num_generation", "-"),
            "V3_Time_s": v3_met.get("runtime_s", "-"),
            "Roi_Time_s": roi_met.get("runtime_s", "-"),
            "Maya_Time_s": maya_met.get("runtime_s", "-"),
            "Shahaf_Time_s": shahaf_met.get("runtime_s", "-"),
            "V3_Speedup": (maya_met.get("runtime_s") / v3_met.get("runtime_s")) if maya_met.get("runtime_s") and v3_met.get("runtime_s") else "-",
            "Roi_Speedup": (maya_met.get("runtime_s") / roi_met.get("runtime_s")) if maya_met.get("runtime_s") and roi_met.get("runtime_s") else "-",
            "V3_Cmp": v3_met.get("cmp_chooseh", "-"),
            "Roi_Cmp": roi_met.get("cmp_chooseh", "-"),
            "Maya_Cmp": maya_met.get("cmp_chooseh", "-")
        }
        
        results.append(row_dict)
        update_md(results)
        
        append_csv([
            row_dict["N"], row_dict["M"], row_dict["Rho"], row_dict["K"],
            row_dict["Sols"], row_dict["Generations"],
            row_dict["Roi_Time_s"], row_dict["Maya_Time_s"], row_dict["Shahaf_Time_s"],
            row_dict["Roi_Speedup"], row_dict["V3_Speedup"], row_dict["Roi_Cmp"], row_dict["Maya_Cmp"], row_dict["V3_Cmp"]
        ])'''
        
    code = code.replace(dict_update, new_dict_update)
    
    # 6. Change logic so Maya runs if V3 OR Roi succeeds, not just Roi
    code = code.replace('if roi_met.get("runtime_s"):', 'if v3_met.get("runtime_s") or roi_met.get("runtime_s"):')
    
    with open('benchmarks/adaptive_explorer.py', 'w') as f:
        f.write(code)
        
if __name__ == "__main__":
    run()

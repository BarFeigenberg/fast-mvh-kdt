# Quick test: can pythonw write a file?
import os, sys
out = open(r"c:\Users\barf9\Desktop\FAST MVH-KDT\benchmarks\runs\pythonw_test.txt", "w")
sys.stdout = out
sys.stderr = out
print("pythonw is alive!")
print(f"sys.executable = {sys.executable}")
print(f"cwd = {os.getcwd()}")
print(f"__file__ = {__file__}")
out.flush()
out.close()

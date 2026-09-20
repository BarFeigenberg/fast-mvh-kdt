---
name: clean-and-build
description: Skill for standardizing C++20 Release builds with -O2 optimization and zero-warning enforcement.
---

# Clean and Build Skill

## Purpose
Configures and compiles the C++20 research codebase (`FastMVH_KDT`) with `-O2` / `/O2` performance optimizations, debug assertion stripping (`-DNDEBUG`), and strict zero-warning enforcement.

## Build Requirements
- **Standard**: C++20 (`-std=c++20` or `/std:c++20`)
- **Optimization**: `-O2` (GCC/Clang) or `/O2` (MSVC)
- **Zero-Warning Policy**: Warning as errors (`/WX` on MSVC, `-Werror` on GCC/Clang)
- **Sanitizers / Asserts**: Disabled during benchmark builds; enabled only in debug / test configurations.

---

## Build Commands

### 1. Windows (MSVC 2026 / Visual Studio BuildTools)

#### Standard Release Build
```powershell
# Configure
& "C:\Program Files\CMake\bin\cmake.exe" -B build -S .

# Compile Release with maximum parallel jobs
& "C:\Program Files\CMake\bin\cmake.exe" --build build --config Release --parallel
```

#### Strict Zero-Warning Release Build
```powershell
& "C:\Program Files\CMake\bin\cmake.exe" -B build -S . -DCMAKE_CXX_FLAGS="/O2 /W4 /WX /permissive- /DNDEBUG"
& "C:\Program Files\CMake\bin\cmake.exe" --build build --config Release --parallel
```

#### Clean Build (Purge and Rebuild)
```powershell
# Clean build artifacts safely without deleting source files
& "C:\Program Files\CMake\bin\cmake.exe" --build build --config Release --target clean
& "C:\Program Files\CMake\bin\cmake.exe" --build build --config Release --parallel
```

---

### 2. Linux / macOS (GCC 11+ or Clang 14+)

```bash
# Configure Release with -O2 and -Werror
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-O2 -Wall -Wextra -Wpedantic -Werror -DNDEBUG"

# Compile with all cores
cmake --build build --parallel $(nproc)
```

---

## Target Verification Checklist

After building, verify that targets are produced in the expected location:
1. Core Library: `build/Release/fast_mvh_lib.lib` (Windows) or `build/libfast_mvh_lib.a` (Linux)
2. Solver Executable: `build/Release/fast_mvh.exe` (Windows) or `build/fast_mvh` (Linux)
3. Execute smoke test:
   ```powershell
   .\build\Release\fast_mvh.exe
   ```
   Must output:
   ```
   Fast MVH with K-d Tree Dominance Checking (C++20)
   Target initialized successfully.
   ```

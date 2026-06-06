# futari_recomp

Static recompilation of **Mushihimesama Futari Ver. 1.5** (Xbox 360, 2006/2012)
for native PC execution, using the [ReXGlue SDK](https://github.com/Subarasheese/rexglue-sdk).

> **This repository contains no copyrighted game files.**
> You must own a legal copy of the game.

---

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| CMake | 3.25+ | |
| Ninja | any | `winget install Ninja-build.Ninja` |
| Clang | 20+ | Windows: install via VS 2022 → "C++ Clang tools for Windows" |
| Python | 3.8+ | For extraction scripts |
| Git | any | For submodules |

### Windows — install Clang via Visual Studio
1. Open **Visual Studio Installer**
2. Modify → Desktop development with C++ → check:
   - **C++ Clang Compiler for Windows**
   - **MSBuild support for LLVM (clang-cl) toolset**

---

## Step-by-step build

### 1. Clone with submodules
```sh
git clone --recurse-submodules https://github.com/YOUR_USER/futari_recomp.git
cd futari_recomp
```

### 2. Extract game files from your ISO
```sh
python3 scripts/extract_iso.py "Mushihimesama Futari Ver. 1.5 [RF].iso" --out assets/
```
This extracts `default.xex` and all game data into `assets/`.

### 3. Analyse the XEX — get addresses for the config
```sh
python3 scripts/analyse_xex.py assets/default.xex
```
Copy the printed addresses into:
- `ppc/ppc_config.h` — `PPC_IMAGE_BASE`, `PPC_CODE_BASE`, sizes
- `config/futari_rexglue.toml` — save/restore stub addresses

Then run XenonAnalyse (from the ReXGlue SDK tools) to generate switch tables:
```sh
XenonAnalyse assets/default.xex config/futari_switch_tables.toml
```
Uncomment `switch_table_file_path` in `config/futari_rexglue.toml` and
`config/futari_manifest.toml`.

### 4. Configure CMake
```sh
# Windows
cmake --preset win-amd64 -S project -B project/out/build/win-amd64

# Linux
cmake --preset linux-amd64 -S project -B project/out/build/linux-amd64
```

### 5. Run codegen (translate PPC → C++)
```sh
cmake --build project/out/build/win-amd64 --target codegen
```
> Note: codegen may exit with signal 11 after printing "done" — that's normal.
> Check that `generated/*.cpp` files were created.

### 6. Build
```sh
cmake --build project/out/build/win-amd64 --config Release
```

### 7. Run
```sh
./project/out/build/win-amd64/Release/futari.exe --game_data_root assets/
```

---

## Project layout

```
futari_recomp/
├── assets/              # gitignored — your extracted game files go here
├── config/
│   ├── futari_manifest.toml    # ReXGlue manifest
│   └── futari_rexglue.toml     # XenonRecomp config (addresses)
├── generated/           # gitignored — codegen output
├── ppc/
│   ├── ppc_config.h     # Image base / size constants
│   └── ppc_detail.h     # Indirect call validator
├── project/
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   └── src/
│       ├── main.cpp     # Entry point / ReXGlue init
│       └── stubs.cpp    # XOS/XAM kernel stubs
├── scripts/
│   ├── extract_iso.py   # XDVDFS ISO extractor
│   └── analyse_xex.py   # XEX header parser / address finder
└── thirdparty/
    └── rexglue-sdk/     # git submodule
```

---

## Credits

- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) — PPC → C++ recompiler
- [ReXGlue SDK](https://github.com/Subarasheese/rexglue-sdk) — Xbox 360 runtime layer
- [daytona-xbla-recomp](https://github.com/Subarasheese/daytona-xbla-recomp) — reference project
- [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp) — Sonic Unleashed reference

Cave / CAVE Interactive Co., Ltd. own all rights to Mushihimesama Futari.

Configure, build, and test the C++17 engine from the repository root. Needs CMake 3.20 or newer, Git, and a C++17 compiler on PATH. The first configure clones GoogleTest at tag v1.15.2. Warnings are -Wall -Wextra, or /W4 on MSVC.

macOS, Linux, and Windows with Visual Studio use the default generator:

    cmake -S cpp -B build/native
    cmake --build build/native
    ctest --test-dir build/native --output-on-failure

Windows without Visual Studio selects NMake Makefiles and stops when nmake is missing. Pass Ninja, with Ninja and the compiler (MinGW g++ is enough) on PATH:

    cmake -S cpp -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build/native
    ctest --test-dir build/native --output-on-failure
replora_core is the static library built from cpp/src. replora_cli and the Emscripten target both link that library. Neither one implements a second cache policy.
Key is std::int32_t, the integer form of the TypeScript number keys in src/simulation/types.ts.

The CLI binary for Ninja and Makefiles is build/native/replora_cli (replora_cli.exe on Windows). A Visual Studio build writes build/native/Release/replora_cli.exe or build/native/Debug/replora_cli.exe. Exit 0 is success. Exit 2 is invalid usage.

```text
replora_cli --capacity 4 --workload looping --requests 100
replora_cli --capacity 4 --workload skewed --requests 100 --policy lru
replora_cli --capacity 4 --workload looping --requests 20 --seed 42 --format json
replora_cli --capacity 4 --workload bursty --requests 20 --format json --events
replora_cli --capacity 8 --workload sequential --requests 50 --seed 7 --format csv
```

`--workload skewed` is frequency-skewed. `frequency-skewed` is accepted as well. `--events` adds access events to json only; without it the shared serializer still runs and each events array is empty. csv columns are policy, workload, capacity, requests, seed, hits, misses, evictions, hitRate, missRate, executionTimeMicros, estimatedMemoryBytes.

The wasm module is `public/wasm/replora.js` plus `replora.wasm`. `src/engine/replora.ts` loads that module and parses JSON. It does not decide hits or evictions.

Install Emscripten once, then activate it in the shell you use for the build:

```text
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
.\emsdk.bat install latest
.\emsdk.bat activate latest
.\emsdk_env.bat
```

On macOS or Linux, use `./emsdk install latest`, `./emsdk activate latest`, and `source ./emsdk_env.sh`. If `emcc` is not on PATH, `npm run build:wasm` prints these steps and exits.

With `emcc` visible:

```text
npm run build:wasm
npm run test:parity
```

`build:wasm` runs `emcmake cmake -S cpp -B build/wasm -G Ninja -DCMAKE_BUILD_TYPE=Release -DREPLORA_WASM=ON`, then builds `replora_wasm`. The same configure is the CMake preset `wasm` in `cpp/CMakePresets.json` (`cmake --preset wasm` from `cpp/`, which needs `EMSDK`).

Link flags on that target: `-sMODULARIZE=1`, `-sEXPORT_ES6=1`, `-sALLOW_MEMORY_GROWTH=1`, `-sEXPORT_NAME=createReplora`, `-sENVIRONMENT=web,node`, `--bind`, and `-fexceptions`. The exported function is `runSimulation(capacity, workload, length, seed, policy, includeEvents)` and returns the shared serializer, or `{"error":{...}}` when the config is invalid. Exceptions are not thrown into JavaScript. `npm run test:parity` compares that JSON with the native CLI and ignores `executionTimeMicros`, `executionTimeMs`, and `estimatedMemoryBytes`. The time fields move between runs. The memory field is a `sizeof` estimate, and wasm32 pointers are 4 bytes while the native build is 64-bit.

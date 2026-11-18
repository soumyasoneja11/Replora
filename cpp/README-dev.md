Configure, build, and test the C++17 engine from the repository root.

    cmake -S cpp -B build/native
    cmake --build build/native
    ctest --test-dir build/native --output-on-failure

Needs CMake 3.20 or newer, a C++17 compiler, and network on first configure.
GoogleTest is fetched at tag v1.15.2. Warnings are -Wall -Wextra, or /W4 on MSVC.
replora_core is the static library built from cpp/src. replora_cli links that library and does not simulate on its own. WASM is still a commented placeholder.
Key is std::int32_t, the integer form of the TypeScript number keys in src/simulation/types.ts.

The CLI binary is build/native/replora_cli (replora_cli.exe on Windows). Exit 0 is success. Exit 2 is invalid usage.

```text
replora_cli --capacity 4 --workload looping --requests 100
replora_cli --capacity 4 --workload skewed --requests 100 --policy lru
replora_cli --capacity 4 --workload looping --requests 20 --seed 42 --format json
replora_cli --capacity 4 --workload bursty --requests 20 --format json --events
replora_cli --capacity 8 --workload sequential --requests 50 --seed 7 --format csv
```

`--workload skewed` is frequency-skewed. `frequency-skewed` is accepted as well. `--events` adds access events to json only; without it the shared serializer still runs and each events array is empty. csv columns are policy, workload, capacity, requests, seed, hits, misses, evictions, hitRate, missRate, executionTimeMicros, estimatedMemoryBytes.

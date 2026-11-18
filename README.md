# REPLORA

A cache-replacement lab. One C++17 engine runs FIFO, LRU, LFU, and Random on one generated trace. A native CLI and a WebAssembly build both compile `cpp/src`. The React app only collects a configuration, calls the WebAssembly module, and draws the returned hits, misses, evictions, cache state, and explanations.

```text
React viewer (src/)
  config, playback, tables
        |
        | runSimulation(capacity, workload, length, seed, policy, includeEvents)
        v
public/wasm/replora.js + replora.wasm
        |
        | cpp/src  (replora_core)
        v
shared C++ engine  <----  replora_cli   (cpp/cli, native)
```

Defaults are workload `looping`, capacity 4, length 100, and seed 42. Workloads are `sequential`, `random`, `looping`, `bursty`, and `frequency-skewed`. Capacity must be at least 1. Length 0 is valid and reports zeros. The longest trace is 10000. Keys are `std::int32_t`, the integer form of the `number` keys in `src/simulation/types.ts`.

## Build and run

Native engine, from the repository root. Needs CMake 3.20 or newer and a C++17 compiler. The first configure fetches GoogleTest v1.15.2.

```text
cmake -S cpp -B build/native
cmake --build build/native
ctest --test-dir build/native --output-on-failure
```

The CLI is `build/native/replora_cli` (`replora_cli.exe` on Windows). Exit 0 is success. Exit 2 is invalid usage.

```text
replora_cli --capacity 4 --workload looping --requests 100
replora_cli --capacity 4 --workload skewed --requests 100 --policy lru
replora_cli --capacity 4 --workload looping --requests 20 --seed 42 --format json
replora_cli --capacity 4 --workload bursty --requests 20 --format json --events
replora_cli --capacity 8 --workload sequential --requests 50 --seed 7 --format csv
```

`skewed` and `frequency-skewed` are the same workload. `--events` adds per-access events to JSON only. CSV columns from the CLI are `policy,workload,capacity,requests,seed,hits,misses,evictions,hitRate,missRate,executionTimeMicros,estimatedMemoryBytes`.

Benchmarks call that CSV mode. Git Bash or another Bash runs the script:

```text
bash scripts/run_benchmarks.sh
```

It writes `results/benchmark_results.csv`: capacities 4, 16, and 64, workloads `looping` and `frequency-skewed`, 10000 requests, seed 42, all four policies. The script adds `experimentId` and places it first. Re-running replaces the timing column and keeps the other columns on the same platform and ABI. `results/README.md` records the machine, the date, and what that file shows.

WebAssembly uses the same `cpp/src`. Install Emscripten, then activate it in the shell (`emsdk_env.bat` on Windows, `source ./emsdk_env.sh` on macOS or Linux). If `emcc` is missing, `npm run build:wasm` prints the install steps and exits 1.

```text
npm run build:wasm
npm run test:parity
```

`build:wasm` configures `build/wasm` with `emcmake` and Ninja, then writes `public/wasm/replora.js` and `public/wasm/replora.wasm`. Parity compares that module with the native CLI and ignores `executionTimeMicros`, `executionTimeMs`, and `estimatedMemoryBytes`.

Viewer:

```text
npm install
npm run dev
npm test
npm run build
```

`npm run dev` serves the Vite app. `npm test` runs the Vitest viewer tests. `npm run build` typechecks and writes `dist/`.

Shorter engine notes live in `cpp/README-dev.md`. The JSON fields are in `docs/result-schema.md`.

## Policies

| Policy | Structures | Lookup | Insert | Evict | Space |
| --- | --- | --- | --- | --- | --- |
| FIFO | `queue` + `unordered_set` | O(1) | O(1) | O(1) | O(C) |
| LRU | `std::list` + `unordered_map<key, list::iterator>`. Front is MRU, back is LRU | O(1) | O(1) | O(1) | O(C) |
| LFU | `unordered_map` of frequency and insertion order. Victim is the minimum frequency, scanned in O(C). No heap | O(1) | O(1) | O(C) | O(C) |
| RANDOM | `vector` + `unordered_map<key, index>`, seeded RNG, swap-and-pop | O(1) | O(1) | O(1) | O(C) |

Rules the engine keeps:

- A FIFO hit does not reorder the queue.
- An LRU hit always moves that node to the front (MRU).
- An LFU tie goes to the earliest insertion. A later reinsert starts again at frequency 1 with a new insertion order.
- Random draws a victim with the shared LCG and removes it by swapping with the last slot, then popping.
- A miss while free slots remain inserts the key and evicts nothing.

## Metrics

Counts come from the events the engine records.

- Requests are the trace length. Hits plus misses equals that length.
- A hit means the key was already resident.
- A miss means it was not. A miss is an eviction only when the cache was full and a victim was removed.
- Hit rate is `100 * hits / requests`. Miss rate is `100 * misses / requests`. Both are 0 when the length is 0. The CLI prints them with `%.17g`, so a rate that is not a short decimal may show a long binary fraction.
- `executionTimeMicros` is `std::chrono::steady_clock` around `access()` and `getCacheState()` only. Explanation strings are built after the clock stops, so formatting is not part of the time. `executionTimeMs` is that value divided by 1000.
- `estimatedMemoryBytes` is not process RSS and not a heap profiler sample. It is `sizeof` plus the containers held after the run:

| Policy | Estimate |
| --- | --- |
| FIFO | `sizeof(FifoPolicy)` + one queued key per resident + one set node (`key` + pointer) per resident + one pointer per set bucket. Deque block padding is ignored. |
| LRU | `sizeof(LruPolicy)` + a list node (`key` + two pointers) per resident + a map node (`key` + `list::iterator` + pointer) per resident + one pointer per map bucket. |
| LFU | `sizeof(LfuPolicy)` + a map node (`key` + frequency entry + pointer) per resident + one pointer per bucket. |
| RANDOM | `sizeof(RandomPolicy)` + `vector.capacity() * sizeof(Key)` + a map node (`key` + index + pointer) per resident + one pointer per bucket. |

## Limitations

The traces are generated. This is not a CPU cache and it does not replay a hardware counter. The engine runs one policy after another on one thread. Memory figures are the estimates above. LFU's victim scan is O(C) on purpose; it is not a heap. Random swap-and-pop does not match the old TypeScript policy, which overwrote the chosen slot, so Random rows in `tests/golden/` are reference-only. The ML panel still in the viewer is a deterministic heuristic in `src/simulation/mlGuided.ts`. It does not run in the C++ engine and it is not part of the measured comparison.

## Migration

The TypeScript simulator, workload generator, metric calculator, and policy classes have been removed. `tests/golden/` remains the recorded fixture set. C++ tests compare FIFO, LRU, and LFU traces, hits, misses, evictions, and cache state with those files. LRU's on-disk order is the old TypeScript order (LRU at index 0); the C++ list is the reverse, front = MRU. The viewer shows the C++ order returned by the engine.

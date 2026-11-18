# Benchmark results

Recorded 19 November 2026 from `scripts/run_benchmarks.sh`.

- Binary: `build/release/replora_cli.exe`
- Build: CMake Release, MinGW-W64 GCC 16.2.0 (x86_64-ucrt-posix-seh)
- Platform: 64-bit Windows 11, build 10.0.26300
- Requests: 10000. Seed: 42. Policies: FIFO, LRU, LFU, RANDOM

`executionTimeMicros` is one capture from that Release binary. The same script used to record a Debug binary; those timings are replaced here. `estimatedMemoryBytes` is the engine's container-size estimate for this 64-bit build, not process RSS. A 32-bit or WebAssembly build uses different pointer widths and will not match these byte counts.

## Why these two workloads

Looping is the engine's fixed cycle of keys 1, 2, 3, and 4, so a capacity of 4 already holds the whole trace. Frequency-skewed draws three hot keys and twelve cold keys, so capacities 4, 16, and 64 sit below that 15-key set, just above it, and well above it. The pair separates "more slots do nothing" from "more slots stop the evictions."

## What this CSV shows

Every looping row is the same outcome: 9996 hits, 4 misses, and 0 evictions, at capacities 4, 16, and 64, for all four policies. Frequency-skewed at capacity 4 is the only experiment where the policies disagree: LFU has 7149 hits, LRU 5759, FIFO 5257, and RANDOM 5136, and each policy's evictions equal its misses minus 4. At capacities 16 and 64 that workload is identical for every policy: 9985 hits, 15 misses, and 0 evictions. Each policy's memory estimate is also the same at those two capacities: FIFO 624, LRU 928, LFU 732, RANDOM 700. Looping estimates stay flat across all three capacities: FIFO 320, LRU 360, LFU 296, RANDOM 304. The estimate therefore follows how many keys were stored, not the configured capacity. Hits plus misses is 10000 on every row. In this Release capture the timing column does not keep one policy order: LFU is slowest in four experiments, FIFO is slowest in `looping-c16` and `frequency-skewed-c4`, and the fastest policy is LRU or RANDOM depending on the row.

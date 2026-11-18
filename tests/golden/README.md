# Golden fixtures

These files are exported from the current TypeScript engine (`runSimulation` in `src/simulation/simulator.ts`). They are the parity target for the C++ engine. Regenerate them from the repo root with:

```powershell
$env:WRITE_GOLDEN = "1"
npx vitest run scripts/exportGolden.test.ts
Remove-Item Env:WRITE_GOLDEN
```

The matrix is 5 workloads × capacities {1, 2, 4, 8} × lengths {0, 1, 20, 100} × seeds {42, 7} (160 files). A file name is `<workload>_c<cap>_n<len>_s<seed>.json`.

`npm test` runs `scripts/exportGolden.test.ts`, which builds the matrix twice and requires the bytes to match, including the files on disk. Execution time is not part of the fixture. `performance.now()` changes between runs, so a timing field would make the bytes unstable.

## What C++ must match

For FIFO, LRU, and LFU these fields are authoritative, byte for byte:

- `config` (`workload`, `capacity`, `traceLength`, `seed`)
- `trace` (the one key array shared by all four policies)
- `policies.FIFO`, `policies.LRU`, and `policies.LFU`:
  - `accesses[]`: `hit`, `evicted`, `victim`, `cacheStateAfter`
  - `hits`, `misses`, `evictions`

`hit` is true when the TypeScript event result is `"hit"`. `evicted` is true only when `victim` is not null. A miss into free space has `evicted: false` and `victim: null`. Length 0 has an empty trace, empty access arrays, and all three counts at 0.

`cacheStateAfter` is the engine's array after that access, not a sorted set.

- FIFO: queue order. Index 0 is the oldest key and the next victim. A hit does not reorder.
- LRU: TypeScript `Map` order. Index 0 is least recently used. The last index is most recently used. A hit deletes the key and inserts it at the end. The C++ list keeps front as MRU and back as LRU, so a front-to-back walk is the reverse of this array. Emit this array's order (LRU at index 0) to match the fixture.
- LFU: resident keys in insertion-list order, not frequency order. A new key is appended. The victim is removed in place and later keys shift down. A hit does not reorder. The victim is the lowest frequency; a tie goes to the earliest insertion. Insertion order is a counter starting at 0, assigned when the key is inserted. A new key starts at frequency 1. A hit increments that frequency by 1.

Trace generation must match exactly, including for workloads that ignore the seed. Sequential and looping do not call the RNG, but the seed is still stored and is still the Random policy's seed.

### Traces

The shared generator is `createSeededRng` in `src/simulation/workloads.ts`. One trace is built, then each policy runs on that same array.

- **sequential:** key `i + 1` for `i` in `0 .. length-1`. Seed is not used.
- **looping:** `1, 2, 3, 4` repeating. Seed is not used.
- **random:** each key is `floor(rng() * 20) + 1` (keys 1 through 20).
- **bursty:** while the trace is short of `length`:
  1. `burstItem = floor(rng() * 10) + 1` (keys 1 through 10)
  2. `burstLen = min(3 + floor(rng() * 8), length - i)` (3 through 10, clipped to what remains)
  3. For each step in the burst, draw `rng()`. If it is `< 0.2`, append `floor(rng() * 10) + 11` (keys 11 through 20). Otherwise append `burstItem`. The second draw happens only on the noise branch.
- **frequency-skewed:** for each index, draw `rng()`. If it is `< 0.7`, append `hot[floor(rng() * 3)]` where hot is `[1, 2, 3]`. Otherwise append `cold[floor(rng() * 12)]` where cold is `[4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]`.

### LCG

Workload RNGs and the Random policy RNG are the same function, with separate state. Both start at the raw seed. The first draw advances first; the seed itself is not a draw.

```text
state = seed
state = (state * 1664525 + 1013904223) mod 2^32
unit = unsigned32(state) / 4294967296
```

Constants: multiplier `1664525`, increment `1013904223`, modulus `2^32` (`0x100000000`). In JavaScript the update is `s = (s * 1664525 + 1013904223) & 0xffffffff`, and the unit interval is `(s >>> 0) / 0x100000000`, which is in `[0, 1)`. `Math.floor(unit * range)` is the integer draw. C++ should use `uint32_t` wrap for the update and a `double` divide by `4294967296.0` for the unit value. An integer remainder such as `state % range` does not match `floor(unit * range)`.

## Random is reference only

`policies.RANDOM` is exported from the TypeScript policy so the fixture is complete. Do not require C++ Random to equal it.

On a miss when the cache is full, TypeScript picks `floor(rng() * items.length)` and writes the new key over that slot (`items[victimIdx] = item`). It does not swap the victim with the last slot and pop. C++ Random must use a vector plus `unordered_map<key, index>`, the seeded RNG, and swap-and-pop. Those two removals choose different victims and leave a different `cacheStateAfter`, so the bytes will differ.

The TypeScript Random RNG starts at the same seed as the trace and advances only when it picks a victim. A hit does not advance it. A miss into free space does not advance it.

Verify C++ Random by determinism (same seed, same victim sequence) and by the cache invariants (capacity, membership, hit does not evict, free-space miss does not evict). Do not verify it by comparing these JSON files.

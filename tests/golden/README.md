# Golden fixtures

These JSON files are frozen regression data recorded from the old TypeScript engine. Do not regenerate them. The TypeScript simulator, its exporter, and `scripts/exportGolden.ts` are gone. Editing a fixture to match a new C++ result would hide a behavior change. If a C++ test disagrees with a file, fix the engine or report the difference. Leave the file as recorded.

The matrix is 5 workloads × capacities {1, 2, 4, 8} × lengths {0, 1, 20, 100} × seeds {42, 7} (160 files). A file name is `<workload>_c<cap>_n<len>_s<seed>.json`.

Timing was never stored. The C++ tests compare the fields below.

## What the C++ tests match

For FIFO, LRU, and LFU these fields are authoritative:

- `config` (`workload`, `capacity`, `traceLength`, `seed`)
- `trace` (the one key array shared by all four policies)
- `policies.FIFO`, `policies.LRU`, and `policies.LFU`:
  - `accesses[]`: `hit`, `evicted`, `victim`, `cacheStateAfter`
  - `hits`, `misses`, `evictions`

`hit` is true when the recorded event was a hit. `evicted` is true only when `victim` is not null. A miss into free space has `evicted: false` and `victim: null`. Length 0 has an empty trace, empty access arrays, and all three counts at 0.

`cacheStateAfter` is the array after that access, not a sorted set.

- FIFO: queue order. Index 0 is the oldest key and the next victim. A hit does not reorder.
- LRU: the old TypeScript `Map` order. Index 0 is least recently used. The last index is most recently used. The C++ list keeps front as MRU and back as LRU, so a front-to-back walk is the reverse of this array. Tests reverse the C++ state before comparing. The viewer shows the C++ order.
- LFU: resident keys in insertion order, not frequency order. A hit does not reorder. The victim is the lowest frequency; a tie goes to the earliest insertion. A new key starts at frequency 1. A reinsert starts again at frequency 1 with a new insertion order.

Sequential and looping traces do not use the seed. The seed is still stored, and it is still the Random policy's seed.

### How the frozen traces were generated

- **sequential:** key `i + 1` for `i` in `0 .. length-1`.
- **looping:** `1, 2, 3, 4` repeating.
- **random:** each key is `floor(unit * 20) + 1` (keys 1 through 20).
- **bursty:** while the trace is short of `length`, pick a burst item from 1 through 10, a burst length from 3 through 10 clipped to what remains, then for each step append that item, or with probability 0.2 a noise key from 11 through 20.
- **frequency-skewed:** with probability 0.7 a hot key from `[1, 2, 3]`, otherwise a cold key from `[4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]`.

The generator was a Numerical Recipes LCG. Workload draws and Random victim draws were separate streams, both starting at the raw seed.

```text
state = seed
state = (state * 1664525 + 1013904223) mod 2^32
unit = unsigned32(state) / 4294967296
```

Integer draws are `floor(unit * range)`. `state % range` does not match that.

## Random is reference only

`policies.RANDOM` is in each file because the old exporter wrote it. Do not require C++ Random to equal it.

On a full-cache miss, the old TypeScript policy wrote the new key over the chosen slot. C++ Random uses a vector plus `unordered_map<key, index>` and swap-and-pop. Those removals choose different victims and leave a different `cacheStateAfter`.

Verify C++ Random by determinism and by the cache invariants: capacity, membership, a hit does not evict, and a miss into free space does not evict. Do not verify it by comparing these JSON files.

# SimulationResult JSON

`replora::toJson` writes this object. The native CLI and the WASM build both call that function, so they emit the same bytes for the same result. Keys stay in the order below. Strings are escaped (`\"`, `\\`, `\b`, `\f`, `\n`, `\r`, `\t`, and `\u00xx` for other bytes below 0x20).

`capacity` and `traceLength` are signed. `capacity` must be >= 1. `traceLength` must be from 0 through 10000 (`kMaxTraceLength`). A failure is a `ConfigError` and does not throw. Length 0 is valid: empty trace, zero hits, misses, and evictions, and both rates are 0.

`estimatedMemoryBytes` is a container-size estimate, not process RSS. `executionTimeMicros` is `std::chrono::steady_clock` around each policy's access loop and state snapshot. Explanation strings and JSON are built after that clock stops. `executionTimeMs` is `executionTimeMicros / 1000`.

```json
{
  "config": {
    "workload": "looping",
    "capacity": 4,
    "traceLength": 100,
    "seed": 42,
    "policySelection": "all",
    "policyName": "",
    "workloadName": ""
  },
  "trace": [1, 2, 3, 4],
  "results": [
    {
      "policyName": "FIFO",
      "metrics": {
        "hits": 0,
        "misses": 0,
        "evictions": 0,
        "hitRate": 0,
        "missRate": 0,
        "executionTimeMs": 0,
        "executionTimeMicros": 0,
        "totalAccesses": 0,
        "cacheCapacity": 4,
        "estimatedMemoryBytes": 0
      },
      "events": [
        {
          "index": 0,
          "key": 1,
          "hit": false,
          "evicted": false,
          "victim": null,
          "stateAfter": {
            "keys": [1],
            "frequencies": [],
            "ordering": "front=oldest"
          },
          "policyName": "FIFO",
          "explanation": "FIFO miss: key 1. ..."
        }
      ]
    }
  ],
  "errors": [
    {"field": "capacity", "message": "capacity must be >= 1"}
  ]
}
```

| JSON field | C++ member | Notes |
| --- | --- | --- |
| config.workload | SimulationConfig.workload | `sequential`, `random`, `looping`, `bursty`, `frequency-skewed` |
| config.capacity | SimulationConfig.capacity | int64, must be >= 1 |
| config.traceLength | SimulationConfig.traceLength | int64, 0..10000 |
| config.seed | SimulationConfig.seed | uint32 |
| config.policySelection | SimulationConfig.policySelection | `all` or `one` |
| config.policyName | SimulationConfig.policyName | `FIFO`, `LRU`, `LFU`, or `RANDOM` when selection is `one` |
| config.workloadName | SimulationConfig.workloadName | optional raw name; unknown text is a workload error |
| trace | SimulationResult.trace | one key array shared by every policy |
| results[].policyName | PolicyResult.policyName | |
| results[].metrics.* | PolicyMetrics | names match the struct, including executionTimeMicros and estimatedMemoryBytes |
| results[].events[].victim | AccessEvent.victim | number, or null when there was no eviction |
| results[].events[].stateAfter | AccessEvent.stateAfter | keys, parallel frequencies (empty except LFU), ordering label |
| results[].events[].explanation | AccessEvent.explanation | built after the timed loop from the reason code, key, victim, and state |
| errors[].field | ConfigError.field | `capacity`, `traceLength`, `workload`, or `policy` |
| errors[].message | ConfigError.message | |

On a validation error, `results` is empty and `trace` is empty. `hitRate` and `missRate` are percents. They sum to 100 when the trace is non-empty, and both are 0 when the length is 0. Hits plus misses equals the trace length. Evictions equals the number of events with `evicted: true`.

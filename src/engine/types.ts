// Mirrors docs/result-schema.md. These types describe JSON from replora::toJson.
// The browser does not compute hits, misses, evictions, or metrics.

export interface SimulationConfig {
  workload: "sequential" | "random" | "looping" | "bursty" | "frequency-skewed";
  capacity: number;
  traceLength: number;
  seed: number;
  policySelection: "all" | "one";
  policyName: string;
  workloadName: string;
}

export interface CacheState {
  keys: number[];
  frequencies: number[];
  ordering: string;
}

export interface AccessEvent {
  index: number;
  key: number;
  hit: boolean;
  evicted: boolean;
  victim: number | null;
  stateAfter: CacheState;
  policyName: string;
  explanation: string;
}

export interface PolicyMetrics {
  hits: number;
  misses: number;
  evictions: number;
  hitRate: number;
  missRate: number;
  executionTimeMs: number;
  executionTimeMicros: number;
  totalAccesses: number;
  cacheCapacity: number;
  estimatedMemoryBytes: number;
}

export interface PolicyResult {
  policyName: string;
  metrics: PolicyMetrics;
  events: AccessEvent[];
}

export interface ConfigError {
  field: string;
  message: string;
}

export interface SimulationResult {
  config: SimulationConfig;
  trace: number[];
  results: PolicyResult[];
  errors: ConfigError[];
}

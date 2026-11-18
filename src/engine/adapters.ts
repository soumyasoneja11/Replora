import type {
  AccessEvent,
  PolicyName,
  PolicyResult,
  SimulationResult,
  WorkloadType,
} from "../simulation/types";
import type {
  AccessEvent as EngineEvent,
  PolicyResult as EnginePolicy,
  SimulationResult as EngineResult,
} from "./types";

const POLICY_NAMES: readonly PolicyName[] = ["FIFO", "LRU", "LFU", "RANDOM"];

function isPolicyName(name: string): name is PolicyName {
  return (POLICY_NAMES as readonly string[]).includes(name);
}

function isWorkload(name: string): name is WorkloadType {
  return (
    name === "sequential" ||
    name === "random" ||
    name === "looping" ||
    name === "bursty" ||
    name === "frequency-skewed"
  );
}

function toEvent(event: EngineEvent): AccessEvent {
  return {
    index: event.index,
    requestedItem: event.key,
    result: event.hit ? "hit" : "miss",
    victim: event.victim,
    cacheState: event.stateAfter.keys,
    explanation: event.explanation,
  };
}

function toPolicy(policy: EnginePolicy): PolicyResult | null {
  if (!isPolicyName(policy.policyName)) {
    return null;
  }
  return {
    policy: policy.policyName,
    metrics: {
      hits: policy.metrics.hits,
      misses: policy.metrics.misses,
      evictions: policy.metrics.evictions,
      hitRate: policy.metrics.hitRate,
      missRate: policy.metrics.missRate,
      executionTimeMs: policy.metrics.executionTimeMs,
      totalAccesses: policy.metrics.totalAccesses,
      cacheCapacity: policy.metrics.cacheCapacity,
      estimatedMemoryBytes: policy.metrics.estimatedMemoryBytes,
    },
    events: policy.events.map(toEvent),
  };
}

// Copies engine fields into the shapes the existing components already render.
// Rates, hits, misses, and evictions are taken from the engine JSON as-is.
export function toSimulationResult(engine: EngineResult): SimulationResult {
  const workload = isWorkload(engine.config.workload) ? engine.config.workload : "looping";
  return {
    config: {
      workload,
      capacity: engine.config.capacity,
      traceLength: engine.config.traceLength,
      seed: engine.config.seed,
    },
    trace: engine.trace,
    results: engine.results.flatMap((policy) => {
      const mapped = toPolicy(policy);
      return mapped === null ? [] : [mapped];
    }),
  };
}

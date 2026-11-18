import { mkdirSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import type { PolicyName, PolicyResult, WorkloadType } from "../src/simulation/types";

export const WORKLOADS: readonly WorkloadType[] = [
  "sequential",
  "random",
  "looping",
  "bursty",
  "frequency-skewed",
];

export const CAPACITIES: readonly number[] = [1, 2, 4, 8];
export const LENGTHS: readonly number[] = [0, 1, 20, 100];
export const SEEDS: readonly number[] = [42, 7];

const POLICY_ORDER: readonly PolicyName[] = ["FIFO", "LRU", "LFU", "RANDOM"];

export interface GoldenAccess {
  hit: boolean;
  evicted: boolean;
  victim: number | null;
  cacheStateAfter: number[];
}

export interface GoldenPolicy {
  accesses: GoldenAccess[];
  hits: number;
  misses: number;
  evictions: number;
}

export interface GoldenFixture {
  config: {
    workload: WorkloadType;
    capacity: number;
    traceLength: number;
    seed: number;
  };
  trace: number[];
  policies: Record<PolicyName, GoldenPolicy>;
}

export interface GoldenFile {
  fileName: string;
  json: string;
}

export function goldenDirectory(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../tests/golden");
}

export function goldenFileName(
  workload: WorkloadType,
  capacity: number,
  traceLength: number,
  seed: number,
): string {
  return `${workload}_c${capacity}_n${traceLength}_s${seed}.json`;
}

function toPolicy(result: PolicyResult): GoldenPolicy {
  return {
    accesses: result.events.map((event) => ({
      hit: event.result === "hit",
      evicted: event.victim !== null,
      victim: event.victim,
      cacheStateAfter: event.cacheState,
    })),
    hits: result.metrics.hits,
    misses: result.metrics.misses,
    evictions: result.metrics.evictions,
  };
}

export function buildFixture(
  _workload: WorkloadType,
  _capacity: number,
  _traceLength: number,
  _seed: number,
): GoldenFixture {
  throw new Error(
    "The TypeScript simulator was removed. tests/golden/ remains the recorded fixture set.",
  );
}

export function serializeFixture(fixture: GoldenFixture): string {
  return `${JSON.stringify(fixture, null, 2)}\n`;
}

export function buildGoldenFiles(): GoldenFile[] {
  const files: GoldenFile[] = [];

  for (const workload of WORKLOADS) {
    for (const capacity of CAPACITIES) {
      for (const traceLength of LENGTHS) {
        for (const seed of SEEDS) {
          files.push({
            fileName: goldenFileName(workload, capacity, traceLength, seed),
            json: serializeFixture(buildFixture(workload, capacity, traceLength, seed)),
          });
        }
      }
    }
  }

  return files;
}

export function writeGoldenFixtures(directory: string = goldenDirectory()): string[] {
  mkdirSync(directory, { recursive: true });
  const written: string[] = [];

  for (const file of buildGoldenFiles()) {
    const target = path.join(directory, file.fileName);
    writeFileSync(target, file.json, "utf8");
    written.push(target);
  }

  return written;
}

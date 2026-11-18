import { useCallback, useEffect, useRef, useState } from "react";
import type { SimulationResult } from "../simulation/types";
import { toSimulationResult } from "./adapters";
import { isReploraError, runReplora, type ReploraRunConfig } from "./replora";

const INITIAL_RUN: ReploraRunConfig = {
  capacity: 4,
  workload: "looping",
  length: 100,
  seed: 42,
  policy: "all",
  includeEvents: true,
};

export interface ReploraRunState {
  result: SimulationResult | null;
  isRunning: boolean;
  error: string | null;
  wasmFailed: boolean;
  run: (config: ReploraRunConfig) => Promise<SimulationResult | null>;
}

export function useReploraRun(): ReploraRunState {
  const [result, setResult] = useState<SimulationResult | null>(null);
  const [isRunning, setIsRunning] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [wasmFailed, setWasmFailed] = useState(false);
  const generation = useRef(0);

  const run = useCallback(async (config: ReploraRunConfig) => {
    const ticket = ++generation.current;
    setIsRunning(true);
    setError(null);
    try {
      const raw = await runReplora({
        capacity: config.capacity,
        workload: config.workload,
        length: config.length,
        seed: config.seed ?? 42,
        policy: config.policy ?? "all",
        includeEvents: true,
      });
      if (ticket !== generation.current) {
        return null;
      }
      if (isReploraError(raw)) {
        setWasmFailed(false);
        setError(`${raw.error.field}: ${raw.error.message}`);
        return null;
      }
      const view = toSimulationResult(raw);
      setResult(view);
      setWasmFailed(false);
      return view;
    } catch (caught) {
      if (ticket !== generation.current) {
        return null;
      }
      const detail = caught instanceof Error ? caught.message : "Unknown load error";
      setWasmFailed(true);
      setError(`The simulation engine failed to load. ${detail}`);
      return null;
    } finally {
      if (ticket === generation.current) {
        setIsRunning(false);
      }
    }
  }, []);

  useEffect(() => {
    void run(INITIAL_RUN);
  }, [run]);

  return { result, isRunning, error, wasmFailed, run };
}

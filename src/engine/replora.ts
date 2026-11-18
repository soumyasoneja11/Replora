import type { ConfigError, SimulationResult } from "./types.ts";

export interface ReploraRunConfig {
  capacity: number;
  workload: string;
  length: number;
  seed?: number;
  policy?: string;
  includeEvents?: boolean;
}

export interface ReploraBoundaryError {
  error: {
    field: string;
    message: string;
    errors: ConfigError[];
  };
}

export type ReploraResult = SimulationResult | ReploraBoundaryError;

interface ReploraModule {
  runSimulation: (
    capacity: number,
    workload: string,
    length: number,
    seed: number,
    policy: string,
    includeEvents: boolean,
  ) => string;
}

interface ReploraFactoryModule {
  default: (options?: { locateFile?: (path: string) => string }) => Promise<ReploraModule>;
}

let modulePromise: Promise<ReploraModule> | undefined;

async function instantiate(): Promise<ReploraModule> {
  if (typeof window !== "undefined") {
    // A direct import() is rewritten by Vite and the emscripten file is not a
    // Vite module. Load the static file from public/wasm as the browser would.
    const load = new Function(
      "return import('/wasm/replora.js')",
    ) as () => Promise<ReploraFactoryModule>;
    const factoryModule = await load();
    return factoryModule.default({
      locateFile: (file: string) => `/wasm/${file}`,
    });
  }
  const specifier = new URL("../../public/wasm/replora.js", import.meta.url).href;
  const factoryModule = (await import(/* @vite-ignore */ specifier)) as ReploraFactoryModule;
  return factoryModule.default();
}

function loadModule(): Promise<ReploraModule> {
  if (modulePromise === undefined) {
    modulePromise = instantiate().catch((error: unknown) => {
      modulePromise = undefined;
      throw error;
    });
  }
  return modulePromise;
}

export function isReploraError(result: ReploraResult): result is ReploraBoundaryError {
  return "error" in result;
}

export async function runReplora(config: ReploraRunConfig): Promise<ReploraResult> {
  const wasm = await loadModule();
  const json = wasm.runSimulation(
    config.capacity,
    config.workload,
    config.length,
    config.seed ?? 42,
    config.policy ?? "all",
    config.includeEvents ?? false,
  );
  return JSON.parse(json) as ReploraResult;
}

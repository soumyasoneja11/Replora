import { spawnSync } from "node:child_process";
import { statSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { performance } from "node:perf_hooks";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const cli =
  process.platform === "win32"
    ? path.join(root, "build", "native", "replora_cli.exe")
    : path.join(root, "build", "native", "replora_cli");
const jsPath = path.join(root, "public", "wasm", "replora.js");
const wasmPath = path.join(root, "public", "wasm", "replora.wasm");

const ignored = new Set(["executionTimeMicros", "executionTimeMs", "estimatedMemoryBytes"]);

const configs = [
  { capacity: 4, workload: "sequential", length: 32, seed: 42, policy: "all", includeEvents: true },
  { capacity: 3, workload: "random", length: 40, seed: 7, policy: "all", includeEvents: true },
  { capacity: 4, workload: "looping", length: 24, seed: 42, policy: "all", includeEvents: true },
  { capacity: 2, workload: "bursty", length: 36, seed: 99, policy: "all", includeEvents: true },
  {
    capacity: 8,
    workload: "frequency-skewed",
    length: 48,
    seed: 42,
    policy: "all",
    includeEvents: true,
  },
];

function fail(message) {
  console.error(message);
  process.exit(1);
}

function normalize(value) {
  if (Array.isArray(value)) {
    return value.map(normalize);
  }
  if (value !== null && typeof value === "object") {
    const out = {};
    for (const key of Object.keys(value)) {
      if (!ignored.has(key)) {
        out[key] = normalize(value[key]);
      }
    }
    return out;
  }
  return value;
}

function same(left, right, trail, diffs) {
  if (typeof left === "number" && typeof right === "number") {
    if (Object.is(left, right) || Math.abs(left - right) <= 1e-9) {
      return;
    }
    diffs.push(`${trail}: ${left} vs ${right}`);
    return;
  }
  if (Array.isArray(left) || Array.isArray(right)) {
    if (!Array.isArray(left) || !Array.isArray(right) || left.length !== right.length) {
      diffs.push(`${trail}: array length ${left?.length} vs ${right?.length}`);
      return;
    }
    for (let index = 0; index < left.length; index += 1) {
      same(left[index], right[index], `${trail}[${index}]`, diffs);
      if (diffs.length > 8) {
        return;
      }
    }
    return;
  }
  if (left !== null && right !== null && typeof left === "object" && typeof right === "object") {
    const keys = new Set([...Object.keys(left), ...Object.keys(right)]);
    for (const key of keys) {
      if (!(key in left) || !(key in right)) {
        diffs.push(`${trail}.${key}: missing on one side`);
        continue;
      }
      same(left[key], right[key], `${trail}.${key}`, diffs);
      if (diffs.length > 8) {
        return;
      }
    }
    return;
  }
  if (left !== right) {
    diffs.push(`${trail}: ${JSON.stringify(left)} vs ${JSON.stringify(right)}`);
  }
}

function runCli(config) {
  const args = [
    "--capacity",
    String(config.capacity),
    "--workload",
    config.workload,
    "--requests",
    String(config.length),
    "--seed",
    String(config.seed),
    "--policy",
    config.policy,
    "--format",
    "json",
  ];
  if (config.includeEvents) {
    args.push("--events");
  }
  const result = spawnSync(cli, args, { encoding: "utf8" });
  if (result.status !== 0) {
    fail(`CLI failed for ${config.workload}: ${result.stderr}`);
  }
  return JSON.parse(result.stdout);
}

try {
  statSync(jsPath);
  statSync(wasmPath);
  statSync(cli);
} catch {
  fail(
    "Missing wasm output or the native CLI. Run npm run build:wasm and cmake --build build/native first.",
  );
}

const { runReplora, isReploraError } = await import("../src/engine/replora.ts");

for (const config of configs) {
  const wasmResult = await runReplora(config);
  if (isReploraError(wasmResult)) {
    fail(`WASM returned an error for ${config.workload}: ${JSON.stringify(wasmResult)}`);
  }
  if (wasmResult.results.length !== 4) {
    fail(`${config.workload} returned ${wasmResult.results.length} policies`);
  }
  const cliResult = runCli(config);
  const diffs = [];
  same(normalize(wasmResult), normalize(cliResult), config.workload, diffs);
  if (diffs.length > 0) {
    fail(`Parity mismatch for ${config.workload}\n${diffs.join("\n")}`);
  }
  console.log(
    `parity ok ${config.workload} policies=${wasmResult.results.map((item) => item.policyName).join(",")}`,
  );
}

const invalid = await runReplora({
  capacity: 0,
  workload: "looping",
  length: 10,
  seed: 42,
  policy: "all",
  includeEvents: false,
});
if (!isReploraError(invalid) || invalid.error.field !== "capacity") {
  fail(`expected a capacity error object, got ${JSON.stringify(invalid)}`);
}
console.log(`invalid config ok field=${invalid.error.field}`);

const started = performance.now();
const large = await runReplora({
  capacity: 4,
  workload: "looping",
  length: 10000,
  seed: 42,
  policy: "all",
  includeEvents: false,
});
const elapsed = performance.now() - started;
if (isReploraError(large) || large.trace.length !== 10000 || large.results.length !== 4) {
  fail("length=10000 run did not return four policies and 10000 keys");
}
const jsBytes = statSync(jsPath).size;
const wasmBytes = statSync(wasmPath).size;
console.log(`replora.js bytes: ${jsBytes}`);
console.log(`replora.wasm bytes: ${wasmBytes}`);
console.log(
  `length=10000 elapsed ms: ${elapsed.toFixed(1)} json bytes: ${Buffer.byteLength(JSON.stringify(large))} events: off`,
);

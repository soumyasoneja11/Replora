import { spawnSync } from "node:child_process";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");

const installSteps =
  process.platform === "win32"
    ? `emcc was not found on PATH.

Install the Emscripten SDK, then activate it in this shell:

  git clone https://github.com/emscripten-core/emsdk.git
  cd emsdk
  .\\emsdk.bat install latest
  .\\emsdk.bat activate latest
  .\\emsdk_env.bat

Then rerun npm run build:wasm.`
    : `emcc was not found on PATH.

Install the Emscripten SDK, then activate it in this shell:

  git clone https://github.com/emscripten-core/emsdk.git
  cd emsdk
  ./emsdk install latest
  ./emsdk activate latest
  source ./emsdk_env.sh

Then rerun npm run build:wasm.`;

function hasCommand(name) {
  const locator = process.platform === "win32" ? "where.exe" : "which";
  const found = spawnSync(locator, [name], { encoding: "utf8" });
  return found.status === 0;
}

if (!hasCommand("emcc") && !hasCommand("emcc.bat")) {
  console.error(installSteps);
  process.exit(1);
}

const configure = spawnSync(
  "emcmake",
  [
    "cmake",
    "-S",
    "cpp",
    "-B",
    "build/wasm",
    "-G",
    "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DREPLORA_WASM=ON",
  ],
  { cwd: root, stdio: "inherit", shell: process.platform === "win32" },
);
if (configure.status !== 0) {
  process.exit(configure.status ?? 1);
}

const build = spawnSync("cmake", ["--build", "build/wasm", "--target", "replora_wasm"], {
  cwd: root,
  stdio: "inherit",
});
process.exit(build.status ?? 1);

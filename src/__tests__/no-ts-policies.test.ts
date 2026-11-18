import { readdirSync, readFileSync, statSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { expect, it } from "vitest";

const sourceRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");

const forbiddenImport =
  /from\s+["'][^"']*(?:\/algorithms\/|algorithms["']|simulation\/simulator|simulation\/workloads|simulation\/metrics|simulation\/mlGuided|simulation\/explanations)/;
const policyClass = /class\s+\w*Policy\b|implements\s+CachePolicy/;

function sourceFiles(directory: string): string[] {
  const found: string[] = [];
  for (const entry of readdirSync(directory)) {
    const fullPath = path.join(directory, entry);
    if (statSync(fullPath).isDirectory()) {
      found.push(...sourceFiles(fullPath));
      continue;
    }
    if (fullPath.endsWith(".ts") || fullPath.endsWith(".tsx")) {
      found.push(fullPath);
    }
  }
  return found;
}

it("src has no TypeScript cache policy or deleted simulator import", () => {
  const failures: string[] = [];
  for (const file of sourceFiles(sourceRoot)) {
    const text = readFileSync(file, "utf8");
    const relative = path.relative(sourceRoot, file);
    const lines = text.split(/\r?\n/);
    lines.forEach((line: string, index: number) => {
      if (forbiddenImport.test(line) || policyClass.test(line)) {
        failures.push(`${relative}:${index + 1}: ${line.trim()}`);
      }
    });
  }
  expect(failures).toEqual([]);
});

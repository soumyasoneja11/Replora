import { readFileSync } from "node:fs";
import path from "node:path";

import { describe, expect, it } from "vitest";

import {
  buildGoldenFiles,
  CAPACITIES,
  goldenDirectory,
  LENGTHS,
  SEEDS,
  WORKLOADS,
  writeGoldenFixtures,
} from "./exportGolden";

if (process.env.WRITE_GOLDEN === "1") {
  writeGoldenFixtures();
}

describe("golden exporter", () => {
  it("produces identical bytes on two runs", () => {
    const first = buildGoldenFiles();
    const second = buildGoldenFiles();
    const expectedCount = WORKLOADS.length * CAPACITIES.length * LENGTHS.length * SEEDS.length;

    expect(first).toHaveLength(expectedCount);
    expect(second.map((file) => file.fileName)).toEqual(first.map((file) => file.fileName));
    expect(second.map((file) => file.json)).toEqual(first.map((file) => file.json));

    const directory = goldenDirectory();
    for (const file of first) {
      const onDisk = readFileSync(path.join(directory, file.fileName), "utf8").replace(/\r\n/g, "\n");
      expect(onDisk).toBe(file.json);
    }
  });
});

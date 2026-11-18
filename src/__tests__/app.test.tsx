// @vitest-environment jsdom

import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { cleanup, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { afterEach, beforeEach, expect, it, vi } from "vitest";
import type { SimulationResult as EngineResult } from "../engine/types";

const fixtureDir = path.join(path.dirname(fileURLToPath(import.meta.url)), "fixtures");

function loadFixture(name: string): EngineResult {
  return JSON.parse(readFileSync(path.join(fixtureDir, name), "utf8")) as EngineResult;
}

const loopingFixture = loadFixture("looping_c4_n100_s42.json");
const sequentialFixture = loadFixture("sequential_c4_n100_s42.json");

vi.mock("../engine/replora", () => ({
  isReploraError: (value: unknown) =>
    typeof value === "object" && value !== null && "error" in value,
  runReplora: vi.fn(),
}));

import App from "../App";
import { runReplora } from "../engine/replora";

function policyMetrics(fixture: EngineResult, name: string) {
  const policy = fixture.results.find((item) => item.policyName === name);
  if (!policy) {
    throw new Error(`fixture missing ${name}`);
  }
  return policy.metrics;
}

function metric(name: string) {
  return document.querySelector(`[data-metric="${name}"]`)?.textContent ?? "";
}

function traceKeys() {
  return [...document.querySelectorAll(".trace-item")].map((node) => node.textContent?.trim() ?? "");
}

beforeEach(() => {
  Element.prototype.scrollIntoView = () => {};
  vi.mocked(runReplora).mockImplementation(async (config) => {
    return config.workload === "sequential" ? sequentialFixture : loopingFixture;
  });
});

afterEach(() => {
  cleanup();
});

it("renders the CLI fixture and keeps it until Run", async () => {
  const fifo = policyMetrics(loopingFixture, "FIFO");
  const sequential = policyMetrics(sequentialFixture, "FIFO");
  render(<App />);

  await waitFor(() => {
    expect(metric("hit-rate")).toBe(`${fifo.hitRate.toFixed(1)}%`);
  });
  expect(metric("miss-rate")).toBe(`${fifo.missRate.toFixed(1)}%`);
  expect(metric("evictions")).toBe(String(fifo.evictions));
  expect(metric("time")).toBe(`${fifo.executionTimeMs.toFixed(2)}ms`);
  expect(metric("requests")).toBe(String(fifo.totalAccesses));
  expect(metric("hits")).toBe(String(fifo.hits));
  expect(metric("misses")).toBe(String(fifo.misses));
  expect(document.querySelector(".comparison-table")?.textContent).toContain(
    String(fifo.estimatedMemoryBytes),
  );
  expect(document.querySelector(".snapshot-value")?.textContent).toBe("Looping");
  expect(screen.queryByText("Last run — press Run to apply config")).toBeNull();

  fireEvent.click(screen.getByRole("radio", { name: /Sequential/ }));
  expect(screen.getByText("Last run — press Run to apply config")).toBeTruthy();
  expect(metric("hit-rate")).toBe(`${fifo.hitRate.toFixed(1)}%`);
  expect(document.querySelector(".snapshot-value")?.textContent).toBe("Looping");

  fireEvent.click(screen.getAllByRole("button", { name: "Run simulation" })[0]);
  await waitFor(() => {
    expect(metric("hit-rate")).toBe(`${sequential.hitRate.toFixed(1)}%`);
  });
  expect(document.querySelector(".snapshot-value")?.textContent).toBe("Sequential");
  expect(metric("evictions")).toBe(String(sequential.evictions));

  fireEvent.click(screen.getByRole("button", { name: "Skip to end of trace" }));
  const last = sequentialFixture.results.find((item) => item.policyName === "FIFO");
  const keys = last?.events.at(-1)?.stateAfter.keys ?? [];
  const slots = [...document.querySelectorAll(".cache-slot")].map((node) => node.textContent);
  expect(slots.slice(0, keys.length).map((slot) => Number(slot))).toEqual(keys);
});

const random42 = loadFixture("random_c4_n20_s42.json");
const random7 = loadFixture("random_c4_n20_s7.json");

function snapshotValues() {
  return [...document.querySelectorAll(".snapshot-value")].map((node) => node.textContent);
}

function expectedTrace(fixture: typeof random42) {
  return fixture.trace.map((key) => String(key));
}

it("changes the trace on the next run when the seed changes and repeats the same seed", async () => {
  vi.mocked(runReplora).mockImplementation(async (config) => {
    if (config.workload === "random" && config.seed === 7) {
      return random7;
    }
    if (config.workload === "random") {
      return random42;
    }
    return loopingFixture;
  });

  render(<App />);
  const fifo = policyMetrics(loopingFixture, "FIFO");
  await waitFor(() => {
    expect(metric("hit-rate")).toBe(`${fifo.hitRate.toFixed(1)}%`);
  });

  fireEvent.click(screen.getByRole("radio", { name: "Random" }));
  fireEvent.change(screen.getByLabelText("Seed"), { target: { value: "7" } });
  expect(document.querySelector(".snapshot-pending")?.textContent).toBe(
    "Last run — press Run to apply config",
  );
  expect(snapshotValues()[3]).toBe("42");
  expect(traceKeys()).toEqual(expectedTrace(loopingFixture));

  fireEvent.click(screen.getAllByRole("button", { name: "Run simulation" })[0]);
  await waitFor(() => {
    expect(snapshotValues()[0]).toBe("Random");
  });
  expect(snapshotValues()[3]).toBe("7");
  expect(traceKeys()).toEqual(expectedTrace(random7));
  const firstRun = traceKeys();

  fireEvent.click(screen.getAllByRole("button", { name: "Run simulation" })[0]);
  await waitFor(() => {
    expect(runReplora).toHaveBeenCalledTimes(3);
  });
  expect(traceKeys()).toEqual(firstRun);

  fireEvent.change(screen.getByLabelText("Seed"), { target: { value: "42" } });
  expect(document.querySelector(".snapshot-pending")).not.toBeNull();
  fireEvent.click(screen.getAllByRole("button", { name: "Run simulation" })[0]);
  await waitFor(() => {
    expect(snapshotValues()[3]).toBe("42");
  });
  expect(traceKeys()).toEqual(expectedTrace(random42));
  expect(traceKeys()).not.toEqual(expectedTrace(random7));
});

it("steps with Previous and Next and the playback keys", async () => {
  render(<App />);
  await waitFor(() => {
    expect(metric("requests")).toBe("100");
  });

  fireEvent.click(screen.getByRole("button", { name: "Skip to end of trace" }));
  expect(document.querySelector(".trace-counter")?.textContent).toBe("100/100");
  expect((screen.getByRole("button", { name: "Next access" }) as HTMLButtonElement).disabled).toBe(true);
  fireEvent.click(screen.getByRole("button", { name: "Next access" }));
  expect(document.querySelector(".trace-counter")?.textContent).toBe("100/100");

  fireEvent.click(screen.getAllByRole("listitem")[0]);
  expect(document.querySelector(".trace-counter")?.textContent).toBe("1/100");
  expect((screen.getByRole("button", { name: "Previous access" }) as HTMLButtonElement).disabled).toBe(true);
  fireEvent.click(screen.getByRole("button", { name: "Previous access" }));
  expect(document.querySelector(".trace-counter")?.textContent).toBe("1/100");

  fireEvent.click(screen.getByRole("button", { name: "Next access" }));
  expect(document.querySelector(".trace-counter")?.textContent).toBe("2/100");

  fireEvent.keyDown(window, { key: "ArrowLeft" });
  expect(document.querySelector(".trace-counter")?.textContent).toBe("1/100");
  fireEvent.keyDown(window, { key: "ArrowLeft" });
  expect(document.querySelector(".trace-counter")?.textContent).toBe("1/100");
  fireEvent.keyDown(window, { key: "ArrowRight" });
  expect(document.querySelector(".trace-counter")?.textContent).toBe("2/100");

  fireEvent.keyDown(window, { key: " " });
  expect(await screen.findByRole("button", { name: "Pause playback" })).toBeTruthy();
  fireEvent.keyDown(window, { key: " " });
  expect(screen.getByRole("button", { name: "Play trace" })).toBeTruthy();

  fireEvent.click(screen.getByRole("button", { name: "Skip to end of trace" }));
  fireEvent.keyDown(window, { key: "r" });
  await waitFor(() => {
    expect(document.querySelector(".trace-counter")?.textContent).not.toBe("100/100");
  });
  fireEvent.keyDown(window, { key: "Escape" });
  expect(screen.getByRole("button", { name: "Play trace" })).toBeTruthy();
  const stopped = document.querySelector(".trace-counter")?.textContent;
  await new Promise((resolve) => setTimeout(resolve, 120));
  expect(document.querySelector(".trace-counter")?.textContent).toBe(stopped);

  const calls = vi.mocked(runReplora).mock.calls.length;
  fireEvent.keyDown(screen.getByLabelText("Seed"), { key: "Enter" });
  expect(vi.mocked(runReplora).mock.calls.length).toBe(calls);
  fireEvent.keyDown(screen.getByLabelText("Seed"), { key: "Enter", ctrlKey: true });
  await waitFor(() => {
    expect(vi.mocked(runReplora).mock.calls.length).toBe(calls + 1);
  });
});

it("shows the current event explanation verbatim", async () => {
  render(<App />);
  await waitFor(() => {
    expect(metric("requests")).toBe("100");
  });
  fireEvent.click(screen.getByRole("button", { name: "Skip to end of trace" }));
  const fifoEvents = loopingFixture.results.find((item) => item.policyName === "FIFO")?.events ?? [];
  const last = fifoEvents.at(-1);
  expect(document.querySelector(".event-explanation")?.textContent).toBe(last?.explanation);
  expect(document.querySelector(".decision-explanation")?.textContent).toBe(last?.explanation);
  expect(document.body.textContent).toContain("About this workload");

  fireEvent.click(screen.getAllByRole("listitem")[0]);
  const first = loopingFixture.results.find((item) => item.policyName === "FIFO")?.events[0];
  expect(document.querySelector(".event-explanation")?.textContent).toBe(first?.explanation);
  expect(document.querySelector(".decision-explanation")?.textContent).toBe(first?.explanation);
});

it("keeps the same results when animation is turned off", async () => {
  render(<App />);
  const fifo = policyMetrics(loopingFixture, "FIFO");
  await waitFor(() => {
    expect(metric("hits")).toBe(String(fifo.hits));
  });
  fireEvent.click(screen.getByRole("button", { name: "Skip to end of trace" }));
  const before = {
    hits: metric("hits"),
    misses: metric("misses"),
    trace: traceKeys(),
  };

  fireEvent.click(screen.getByRole("button", { name: "Anim ON" }));
  fireEvent.click(screen.getAllByRole("button", { name: "Run simulation" })[0]);
  await waitFor(() => {
    expect(vi.mocked(runReplora).mock.calls.length).toBe(2);
  });
  expect(metric("hits")).toBe(before.hits);
  expect(metric("misses")).toBe(before.misses);
  expect(traceKeys()).toEqual(before.trace);
  const sent = vi.mocked(runReplora).mock.calls.map((call) => call[0]);
  expect(sent.every((config) => config.seed === 42 && config.workload === "looping")).toBe(true);
  expect(sent[0]).not.toHaveProperty("animation");
});

it("shows a WASM load failure and does not invent metrics", async () => {
  vi.mocked(runReplora).mockRejectedValue(new Error("replora.wasm missing"));
  render(<App />);
  const alert = await screen.findByRole("alert");
  expect(alert.textContent).toContain("Simulation engine failed to load");
  expect(alert.textContent).toContain("replora.wasm missing");
  expect(document.querySelector(".metric-strip")).toBeNull();
});

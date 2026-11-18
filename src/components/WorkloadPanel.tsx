import type { WorkloadType } from "../simulation/types";

const WORKLOAD_LABELS: Record<WorkloadType, string> = {
  sequential: "Sequential",
  random: "Random",
  looping: "Looping",
  bursty: "Bursty",
  "frequency-skewed": "Frequency-skewed",
};

const WORKLOAD_SIGNATURES: Record<WorkloadType, string> = {
  sequential: "→ → → → →",
  random: "↗ ↓ ← ↘ ↑",
  looping: "↻",
  bursty: "▂▇▇▇▂▂▇▇",
  "frequency-skewed": "▇▇▇▇▂▁",
};

const ALL_WORKLOADS: WorkloadType[] = [
  "sequential",
  "random",
  "looping",
  "bursty",
  "frequency-skewed",
];

interface WorkloadPanelProps {
  workload: WorkloadType;
  capacity: number;
  traceLength: number;
  seed: string;
  seedValid: boolean;
  animationOn: boolean;
  onWorkloadChange: (w: WorkloadType) => void;
  onCapacityChange: (c: number) => void;
  onTraceLengthChange: (l: number) => void;
  onSeedChange: (seed: string) => void;
  onAnimationToggle: () => void;
  onReset: () => void;
  onRun: () => void;
  isRunning: boolean;
}

export function WorkloadPanel({
  workload,
  capacity,
  traceLength,
  seed,
  seedValid,
  animationOn,
  onWorkloadChange,
  onCapacityChange,
  onTraceLengthChange,
  onSeedChange,
  onAnimationToggle,
  onReset,
  onRun,
  isRunning,
}: WorkloadPanelProps) {
  return (
    <div className="panel workload-panel">
      <div className="panel-header">
        <span className="panel-title">Configuration</span>
      </div>

      <div className="control-group">
        <label className="control-label" id="workload-label">Workload</label>
        <div className="workload-options" role="radiogroup" aria-labelledby="workload-label">
          {ALL_WORKLOADS.map((w) => (
            <button
              key={w}
              className={`workload-btn ${w === workload ? "active" : ""}`}
              onClick={() => onWorkloadChange(w)}
              role="radio"
              aria-checked={w === workload}
            >
              <span>{WORKLOAD_LABELS[w]}</span>
              <span className="workload-signature" aria-hidden="true">
                {WORKLOAD_SIGNATURES[w]}
              </span>
            </button>
          ))}
        </div>
      </div>

      <div className="control-row">
        <div className="control-group">
          <label className="control-label" htmlFor="capacity-input">Cache Size</label>
          <input
            id="capacity-input"
            className="control-input"
            type="number"
            min={1}
            max={20}
            value={capacity}
            onChange={(e) => {
              const v = parseInt(e.target.value, 10);
              if (v >= 1 && v <= 20) onCapacityChange(v);
            }}
          />
        </div>
        <div className="control-group">
          <label className="control-label" htmlFor="trace-input">Trace Length</label>
          <input
            id="trace-input"
            className="control-input"
            type="number"
            min={1}
            max={10000}
            value={traceLength}
            onChange={(e) => {
              const v = parseInt(e.target.value, 10);
              if (v >= 1 && v <= 10000) onTraceLengthChange(v);
            }}
          />
        </div>
        <div className="control-group">
          <label className="control-label" htmlFor="seed-input">Seed</label>
          <input
            id="seed-input"
            className="control-input"
            type="text"
            inputMode="numeric"
            value={seed}
            aria-invalid={!seedValid}
            onChange={(e) => onSeedChange(e.target.value)}
          />
          {!seedValid && (
            <span className="control-error">Enter an integer from 0 through 4294967295.</span>
          )}
        </div>
      </div>

      <div className="control-actions">
        <button
          className={`btn-run ${isRunning ? "running" : ""}`}
          onClick={onRun}
          disabled={isRunning || !seedValid}
          style={{ flex: 1 }}
        >
          {isRunning ? "Running..." : "Run Simulation"}
        </button>
        <button className="btn-reset" onClick={onReset}>
          Reset
        </button>
        <button
          className={`anim-toggle ${animationOn ? "active" : ""}`}
          onClick={onAnimationToggle}
          aria-pressed={animationOn}
        >
          Anim {animationOn ? "ON" : "OFF"}
        </button>
      </div>
    </div>
  );
}

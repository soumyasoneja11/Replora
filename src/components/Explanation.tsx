import type { WorkloadType } from "../simulation/types";

const WORKLOAD_NOTES: Record<WorkloadType, string> = {
  sequential:
    "Sequential workload. The workload continually advances through new entries, creating scan-like pressure on the cache. No policy benefits from reuse since every access is unique.",
  random:
    "Random workload. Weak locality makes historical access patterns less predictable. Policies that track recency or frequency gain little advantage over random eviction.",
  looping:
    "Looping workload. Repeated reuse of a small working set makes recent-access history important. Policies that retain recently used items can capture the entire loop.",
  bursty:
    "Bursty workload. Short periods of concentrated reuse create temporary working sets. Policies sensitive to recent activity can exploit burst locality before the pattern shifts.",
  "frequency-skewed":
    "Frequency-skewed workload. A few entries dominate accesses, making frequency history especially relevant. Policies that retain frequently used items achieve substantially higher hit rates.",
};

interface ExplanationProps {
  workload: WorkloadType;
  explanation: string | null;
}

export function Explanation({ workload, explanation }: ExplanationProps) {
  return (
    <div className="panel">
      <div className="panel-header">
        <span className="panel-title">Why This Happened</span>
      </div>
      <div className="explanation-text">
        <span className="explanation-workload">This access</span>
        <span className="explanation-body event-explanation">
          {explanation ?? "Step to an access to read the engine explanation."}
        </span>
        <span className="explanation-workload">About this workload</span>
        <span className="explanation-note">{WORKLOAD_NOTES[workload]}</span>
      </div>
    </div>
  );
}

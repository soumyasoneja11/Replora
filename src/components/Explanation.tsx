import type { WorkloadType } from "../simulation/types";
import { getExplanation } from "../simulation/explanations";

const WORKLOAD_DISPLAY: Record<WorkloadType, string> = {
  sequential: "Sequential workload",
  random: "Random workload",
  looping: "Looping workload",
  bursty: "Bursty workload",
  "frequency-skewed": "Frequency-skewed workload",
};

interface ExplanationProps {
  workload: WorkloadType;
  explanation: string | null;
}

export function Explanation({ workload, explanation }: ExplanationProps) {
  const workloadNote = getExplanation(workload);

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
        <span className="explanation-note">{WORKLOAD_DISPLAY[workload]}. {workloadNote}</span>
      </div>
    </div>
  );
}

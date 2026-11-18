import type { SimulationMetrics } from "../simulation/types";
import { Tooltip } from "./Tooltip";

interface MetricStripProps {
  metrics: SimulationMetrics | null;
}

export function MetricStrip({ metrics }: MetricStripProps) {
  const requests = metrics ? metrics.totalAccesses.toString() : "—";
  const hits = metrics ? metrics.hits.toString() : "—";
  const misses = metrics ? metrics.misses.toString() : "—";
  const hitRate = metrics ? metrics.hitRate.toFixed(1) : "—";
  const missRate = metrics ? metrics.missRate.toFixed(1) : "—";
  const evictions = metrics ? metrics.evictions.toString() : "—";
  const execTime = metrics ? metrics.executionTimeMs.toFixed(2) : "—";

  return (
    <div className="metric-strip" role="region" aria-label="Simulation metrics">
      <div className="metric-card">
        <Tooltip text="Number of requests in the shared trace.">
          <div className="metric-label">Requests</div>
        </Tooltip>
        <div className="metric-value" data-metric="requests">{requests}</div>
      </div>
      <div className="metric-card">
        <Tooltip text="Requests the engine counted as hits.">
          <div className="metric-label">Hits</div>
        </Tooltip>
        <div className="metric-value" data-metric="hits">{hits}</div>
      </div>
      <div className="metric-card">
        <Tooltip text="Requests the engine counted as misses.">
          <div className="metric-label">Misses</div>
        </Tooltip>
        <div className="metric-value" data-metric="misses">{misses}</div>
      </div>
      <div className="metric-card">
        <Tooltip text="Percentage of accesses already present in the cache.">
          <div className="metric-label">Hit Rate</div>
        </Tooltip>
        <div className="metric-value hit" data-metric="hit-rate">
          {hitRate}<span className="metric-unit">%</span>
        </div>
      </div>
      <div className="metric-card">
        <Tooltip text="Percentage of accesses that required fetching from memory.">
          <div className="metric-label">Miss Rate</div>
        </Tooltip>
        <div className="metric-value miss" data-metric="miss-rate">
          {missRate}<span className="metric-unit">%</span>
        </div>
      </div>
      <div className="metric-card">
        <Tooltip text="Number of entries evicted to make room for new items.">
          <div className="metric-label">Evictions</div>
        </Tooltip>
        <div className="metric-value" data-metric="evictions">{evictions}</div>
      </div>
      <div className="metric-card">
        <Tooltip text="Engine time around the access loop, in milliseconds.">
          <div className="metric-label">Execution</div>
        </Tooltip>
        <div className="metric-value" data-metric="time">
          {execTime}<span className="metric-unit">ms</span>
        </div>
      </div>
    </div>
  );
}

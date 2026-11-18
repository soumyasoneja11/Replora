interface HeaderProps {
  onRun: () => void;
  isRunning: boolean;
  isPlaying: boolean;
  runDisabled?: boolean;
}

export function Header({ onRun, isRunning, isPlaying, runDisabled = false }: HeaderProps) {
  return (
    <header className="header">
      <div className="header-brand">
        <div className="header-title">REPLORA</div>
        <div className="header-subtitle">Cache Replacement Intelligence Lab</div>
        <p className="header-explainer">
          A cache keeps a small set of items close by so the next lookup can avoid a slower store.
          When that set is full, a replacement policy chooses which resident to remove.
          REPLORA sends one request trace through FIFO, LRU, LFU, and Random and shows the hits, misses, and evictions the engine returns.
        </p>
      </div>
      <div className="header-right">
        <span className="header-status" aria-label="Local simulation mode active">
          <span
            className={`status-dot${isPlaying ? " pulsing" : ""}`}
            aria-hidden="true"
          />
          Local Simulation
        </span>
        <button
          className={`btn-run ${isRunning ? "running" : ""}`}
          onClick={onRun}
          disabled={isRunning || runDisabled}
          aria-label="Run simulation"
        >
          {isRunning ? "Running..." : "Run Simulation"}
        </button>
      </div>
    </header>
  );
}

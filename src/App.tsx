import { useState, useCallback, useRef, useEffect } from "react";
import type {
  WorkloadType,
  PolicyName,
  SimulationResult,
} from "./simulation/types";
import { useReploraRun } from "./engine/useReploraRun";
import { Header } from "./components/Header";
import { ExperimentSnapshot } from "./components/ExperimentSnapshot";
import { MetricStrip } from "./components/MetricStrip";
import { PolicyComparison } from "./components/PolicyComparison";
import { WorkloadPanel } from "./components/WorkloadPanel";
import { TraceViewer } from "./components/TraceViewer";
import { CacheState } from "./components/CacheState";
import { Explanation } from "./components/Explanation";
import { ComplexityPanel } from "./components/ComplexityPanel";

const DEFAULTS = {
  workload: "looping" as WorkloadType,
  capacity: 4,
  traceLength: 100,
  seed: 42,
};

function parseSeed(text: string): number | null {
  if (!/^\d+$/.test(text)) {
    return null;
  }
  const value = Number(text);
  if (!Number.isSafeInteger(value) || value > 4294967295) {
    return null;
  }
  return value;
}

function App() {
  const [workload, setWorkload] = useState<WorkloadType>(DEFAULTS.workload);
  const [capacity, setCapacity] = useState(DEFAULTS.capacity);
  const [traceLength, setTraceLength] = useState(DEFAULTS.traceLength);
  const [seedText, setSeedText] = useState(String(DEFAULTS.seed));
  const [animationOn, setAnimationOn] = useState(true);
  const [selectedPolicy, setSelectedPolicy] = useState<PolicyName>("FIFO");
  const [focusedPolicy, setFocusedPolicy] = useState<PolicyName | null>(null);

  const { result: simResult, isRunning, error, wasmFailed, run } = useReploraRun();

  const [playbackIndex, setPlaybackIndex] = useState(DEFAULTS.traceLength - 1);
  const [isPlaying, setIsPlaying] = useState(false);
  const playIntervalRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const playOnNextResult = useRef(true);
  const appliedResult = useRef<SimulationResult | null>(null);
  const playbackEpoch = useRef(0);
  const resultEpoch = useRef(0);
  const seenResult = useRef<SimulationResult | null>(null);
  const animationOnRef = useRef(animationOn);
  animationOnRef.current = animationOn;

  if (simResult !== seenResult.current) {
    seenResult.current = simResult;
    resultEpoch.current = playbackEpoch.current;
  }

  const stopPlayback = useCallback(() => {
    if (playIntervalRef.current) {
      clearInterval(playIntervalRef.current);
      playIntervalRef.current = null;
    }
    setIsPlaying(false);
  }, []);

  const claimPlayback = useCallback(() => {
    playbackEpoch.current += 1;
  }, []);

  const startPlayback = useCallback((result: SimulationResult, fromIndex: number) => {
    const epoch = ++playbackEpoch.current;
    stopPlayback();
    let idx = fromIndex;
    const len = result.trace.length;
    const speed = len > 1000 ? 5 : len > 500 ? 10 : len > 200 ? 20 : len > 50 ? 50 : 80;
    const interval = setInterval(() => {
      if (playbackEpoch.current !== epoch) {
        clearInterval(interval);
        if (playIntervalRef.current === interval) {
          playIntervalRef.current = null;
        }
        return;
      }
      setPlaybackIndex(idx);
      idx++;
      if (idx >= len) {
        clearInterval(interval);
        if (playIntervalRef.current === interval) {
          playIntervalRef.current = null;
        }
        setIsPlaying(false);
      }
    }, speed);
    playIntervalRef.current = interval;
    setIsPlaying(true);
  }, [stopPlayback]);

  useEffect(() => {
    if (!simResult || simResult === appliedResult.current) {
      return;
    }
    appliedResult.current = simResult;
    if (playbackEpoch.current !== resultEpoch.current) {
      return;
    }
    const shouldPlay =
      playOnNextResult.current &&
      animationOnRef.current &&
      simResult.trace.length > 0 &&
      simResult.trace.length <= 2000;
    if (shouldPlay) {
      setPlaybackIndex(-1);
      startPlayback(simResult, 0);
    } else {
      stopPlayback();
      setPlaybackIndex(Math.max(simResult.trace.length - 1, 0));
    }
  }, [simResult, startPlayback, stopPlayback]);

  const parsedSeed = parseSeed(seedText);

  const handleRun = useCallback(() => {
    if (parsedSeed === null) {
      return;
    }
    claimPlayback();
    stopPlayback();
    playOnNextResult.current = animationOn && traceLength <= 2000;
    void run({
      workload,
      capacity,
      length: traceLength,
      seed: parsedSeed,
      policy: "all",
      includeEvents: true,
    });
  }, [workload, capacity, traceLength, parsedSeed, animationOn, claimPlayback, stopPlayback, run]);

  const handleReset = useCallback(() => {
    claimPlayback();
    stopPlayback();
    setWorkload(DEFAULTS.workload);
    setCapacity(DEFAULTS.capacity);
    setTraceLength(DEFAULTS.traceLength);
    setSeedText(String(DEFAULTS.seed));
    setSelectedPolicy("FIFO");
    setFocusedPolicy(null);
    playOnNextResult.current = false;
    void run({
      workload: DEFAULTS.workload,
      capacity: DEFAULTS.capacity,
      length: DEFAULTS.traceLength,
      seed: DEFAULTS.seed,
      policy: "all",
      includeEvents: true,
    });
  }, [claimPlayback, stopPlayback, run]);

  const traceLengthNow = simResult?.trace.length ?? 0;

  const handlePlay = useCallback(() => {
    if (!simResult) {
      return;
    }
    const startIdx = playbackIndex >= simResult.trace.length - 1 ? 0 : Math.max(playbackIndex, 0);
    startPlayback(simResult, startIdx);
  }, [playbackIndex, simResult, startPlayback]);

  const handlePause = useCallback(() => {
    claimPlayback();
    stopPlayback();
  }, [claimPlayback, stopPlayback]);

  const handleSkipToEnd = useCallback(() => {
    claimPlayback();
    stopPlayback();
    setPlaybackIndex(Math.max(traceLengthNow - 1, 0));
  }, [claimPlayback, stopPlayback, traceLengthNow]);

  const handleStep = useCallback((delta: number) => {
    claimPlayback();
    stopPlayback();
    setPlaybackIndex((prev) => {
      if (traceLengthNow <= 0) {
        return 0;
      }
      const current = prev < 0 ? 0 : prev;
      return Math.max(0, Math.min(traceLengthNow - 1, current + delta));
    });
  }, [claimPlayback, stopPlayback, traceLengthNow]);

  const handleRestartPlayback = useCallback(() => {
    if (!simResult || simResult.trace.length === 0) {
      return;
    }
    startPlayback(simResult, 0);
  }, [simResult, startPlayback]);

  const handleScrub = useCallback((index: number) => {
    claimPlayback();
    stopPlayback();
    setPlaybackIndex(Math.max(0, Math.min(index, Math.max(traceLengthNow - 1, 0))));
  }, [claimPlayback, stopPlayback, traceLengthNow]);

  const handleFocusPolicy = useCallback((p: PolicyName | null) => {
    setFocusedPolicy(p);
    if (p) setSelectedPolicy(p);
  }, []);

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if (e.key === "Enter" && (e.ctrlKey || e.metaKey)) {
        e.preventDefault();
        handleRun();
        return;
      }

      const target = e.target as HTMLElement;
      if (target.tagName === "INPUT" || target.tagName === "TEXTAREA") return;

      switch (e.key) {
        case " ":
          e.preventDefault();
          if (isPlaying) handlePause();
          else handlePlay();
          break;
        case "r":
        case "R":
          if (!e.ctrlKey && !e.metaKey) {
            e.preventDefault();
            handleRestartPlayback();
          }
          break;
        case "Escape":
          e.preventDefault();
          handlePause();
          break;
        case "ArrowLeft":
          e.preventDefault();
          handleStep(-1);
          break;
        case "ArrowRight":
          e.preventDefault();
          handleStep(1);
          break;
      }
    };

    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, [isPlaying, handlePause, handlePlay, handleRun, handleRestartPlayback, handleStep]);

  useEffect(() => {
    return () => {
      if (playIntervalRef.current) clearInterval(playIntervalRef.current);
    };
  }, []);

  const status = (wasmFailed || error) && (
    <div className="panel" role="alert" aria-label={wasmFailed ? "Simulation engine failed to load" : "Simulation was not updated"}>
      <div className="panel-header">
        <span className="panel-title">
          {wasmFailed ? "Simulation engine failed to load" : "Simulation was not updated"}
        </span>
      </div>
      <p className="explanation-text">{error}</p>
    </div>
  );

  if (!simResult) {
    return (
      <div className="dashboard">
        <Header onRun={handleRun} isRunning={isRunning} isPlaying={isPlaying} runDisabled={parsedSeed === null} />
        {status ?? (
          <div className="panel" role="status" aria-label="Loading simulation engine">
            <div className="panel-header">
              <span className="panel-title">Loading simulation engine</span>
            </div>
            <p className="explanation-text">Running the default workload.</p>
          </div>
        )}
      </div>
    );
  }

  const policyResult = simResult.results.find(
    (r) => r.policy === selectedPolicy
  );
  const currentEvent =
    policyResult && playbackIndex >= 0
      ? policyResult.events[playbackIndex] ?? null
      : null;

  const selectedMetrics = policyResult?.metrics ?? null;
  const configPending =
    workload !== simResult.config.workload ||
    capacity !== simResult.config.capacity ||
    traceLength !== simResult.config.traceLength ||
    parsedSeed === null ||
    parsedSeed !== (simResult.config.seed ?? DEFAULTS.seed);

  return (
    <div className="dashboard">
      <Header onRun={handleRun} isRunning={isRunning} isPlaying={isPlaying} runDisabled={parsedSeed === null} />
      {status}

      <ExperimentSnapshot
        workload={simResult.config.workload}
        capacity={simResult.config.capacity}
        traceLength={simResult.config.traceLength}
        seed={simResult.config.seed ?? DEFAULTS.seed}
        pending={configPending}
      />

      <MetricStrip metrics={selectedMetrics} />

      <div className="main-grid">
        <PolicyComparison
          results={simResult.results}
          focusedPolicy={focusedPolicy}
          onFocusPolicy={handleFocusPolicy}
        />
        <WorkloadPanel
          workload={workload}
          capacity={capacity}
          traceLength={traceLength}
          seed={seedText}
          seedValid={parsedSeed !== null}
          animationOn={animationOn}
          onWorkloadChange={setWorkload}
          onCapacityChange={setCapacity}
          onTraceLengthChange={setTraceLength}
          onSeedChange={setSeedText}
          onAnimationToggle={() => setAnimationOn((v) => !v)}
          onReset={handleReset}
          onRun={handleRun}
          isRunning={isRunning}
        />
      </div>

      <div className="mobile-sticky-run">
        <button
          className={`btn-run ${isRunning ? "running" : ""}`}
          onClick={handleRun}
          disabled={isRunning || parsedSeed === null}
          aria-label="Run simulation"
        >
          {isRunning ? "Running..." : "Run Simulation"}
        </button>
      </div>

      <div className="trace-cache-row">
        <TraceViewer
          trace={simResult.trace}
          events={policyResult?.events ?? []}
          playbackIndex={playbackIndex}
          isPlaying={isPlaying}
          onPlay={handlePlay}
          onPause={handlePause}
          onPrevious={() => handleStep(-1)}
          onNext={() => handleStep(1)}
          onSkipToEnd={handleSkipToEnd}
          onScrub={handleScrub}
        />
        <CacheState
          capacity={simResult.config.capacity}
          currentEvent={currentEvent}
          selectedPolicy={selectedPolicy}
          onPolicyChange={setSelectedPolicy}
          allResults={simResult.results}
          playbackIndex={playbackIndex}
        />
      </div>

      <div className="bottom-grid">
        <Explanation
          workload={simResult.config.workload}
          explanation={currentEvent?.explanation ?? null}
        />
        <ComplexityPanel />
      </div>

      <div className="keyboard-hints" aria-label="Keyboard shortcuts">
        <span><span className="kbd">Space</span> Play/Pause</span>
        <span><span className="kbd">&larr;</span><span className="kbd">&rarr;</span> Step</span>
        <span><span className="kbd">R</span> Restart playback</span>
        <span><span className="kbd">Esc</span> Stop playback</span>
        <span><span className="kbd">Ctrl</span><span className="kbd">Enter</span> Run</span>
      </div>

      <footer className="footer" aria-label="Application info">
        <span>REPLORA</span>
        <span className="footer-sep" aria-hidden="true">&middot;</span>
        <span>Local Experiment</span>
        <span className="footer-sep" aria-hidden="true">&middot;</span>
        <span>v0.1</span>
      </footer>
    </div>
  );
}

export default App;

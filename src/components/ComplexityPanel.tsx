export function ComplexityPanel() {
  const policies = [
    {
      name: "FIFO",
      strategy: "queue + unordered_set",
      lookup: "O(1)",
      insert: "O(1)",
      evict: "O(1)",
      space: "O(C)",
    },
    {
      name: "LRU",
      strategy: "std::list + unordered_map",
      lookup: "O(1)",
      insert: "O(1)",
      evict: "O(1)",
      space: "O(C)",
    },
    {
      name: "LFU",
      strategy: "unordered_map, O(C) min-frequency scan",
      lookup: "O(1)",
      insert: "O(1)",
      evict: "O(C)",
      space: "O(C)",
    },
    {
      name: "RANDOM",
      strategy: "vector + unordered_map, swap-and-pop",
      lookup: "O(1)",
      insert: "O(1)",
      evict: "O(1)",
      space: "O(C)",
    },
  ];

  return (
    <div className="panel">
      <div className="panel-header">
        <span className="panel-title">Algorithm Complexity</span>
      </div>
      <div className="complexity-grid">
        {policies.map((p) => (
          <div key={p.name} className="complexity-item">
            <span className="complexity-name">{p.name}</span>
            <span className="complexity-strategy">{p.strategy}</span>
            <span className="complexity-ops">
              Lookup {p.lookup} · Insert {p.insert} · Evict {p.evict} · Space {p.space}
            </span>
          </div>
        ))}
      </div>
    </div>
  );
}

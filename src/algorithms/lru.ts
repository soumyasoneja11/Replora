import type { AccessEvent, CachePolicy } from "../simulation/types";

/**
 * LRU via an insertion-ordered Map: lookup, recency update, and eviction
 * of the least-recent key are all O(1).
 */
export class LRUPolicy implements CachePolicy {
  readonly name = "LRU" as const;
  private order = new Map<number, true>();
  private accessIndex = 0;
  readonly capacity: number;

  constructor(capacity: number) {
    this.capacity = capacity;
  }

  access(item: number): AccessEvent {
    const index = this.accessIndex++;

    if (this.order.has(item)) {
      this.order.delete(item);
      this.order.set(item, true);

      return {
        index,
        requestedItem: item,
        result: "hit",
        victim: null,
        cacheState: this.getState(),
      };
    }

    let victim: number | null = null;
    if (this.order.size >= this.capacity) {
      victim = this.order.keys().next().value!;
      this.order.delete(victim);
    }

    this.order.set(item, true);

    return {
      index,
      requestedItem: item,
      result: "miss",
      victim,
      cacheState: this.getState(),
    };
  }

  getState(): number[] {
    return [...this.order.keys()];
  }

  reset(): void {
    this.order.clear();
    this.accessIndex = 0;
  }
}

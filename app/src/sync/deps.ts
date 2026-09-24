// Side-effecting dependencies the services take by injection so tests can
// pin time and ids. The app wires the real ones in src/platform.
export interface SyncDeps {
  /** Unix seconds. */
  now: () => number;
  /** RFC 4122 UUID string. */
  uuid: () => string;
  log: (message: string) => void;
}

export function uuidToBytes(uuid: string): Uint8Array {
  const hex = uuid.replace(/-/g, '');
  if (hex.length !== 32) throw new Error(`not a uuid: ${uuid}`);
  const out = new Uint8Array(16);
  for (let i = 0; i < 16; i++) out[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
  return out;
}

/** Deterministic deps for tests: time starts at `start` and ids count up. */
export function testDeps(start = 1_790_121_600): SyncDeps & { clock: { value: number }; logs: string[] } {
  const clock = { value: start };
  const logs: string[] = [];
  let n = 0;
  return {
    clock,
    logs,
    now: () => clock.value,
    uuid: () => {
      n++;
      return `00000000-0000-4000-8000-${n.toString(16).padStart(12, '0')}`;
    },
    log: (m) => logs.push(m),
  };
}

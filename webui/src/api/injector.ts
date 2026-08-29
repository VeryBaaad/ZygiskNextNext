import { apiBinary, execJson } from './ksu';

export interface InjectorStatus {
  running: boolean;
  pid: number;
  zygiskCompat: boolean;
  mode?: string;
}

export async function getInjectorStatus(): Promise<InjectorStatus> {
  const raw = await execJson<{
    running?: unknown;
    pid?: number;
    zygiskCompat?: unknown;
    mode?: string;
  }>(`'${apiBinary()}' --ctl status`);
  // Defensive normalization: tolerate string "true"/"false" as well as real
  // booleans, so a malformed snapshot can never crash the status card.
  return {
    pid: typeof raw.pid === 'number' ? raw.pid : 0,
    mode: typeof raw.mode === 'string' ? raw.mode : undefined,
    running: raw.running === true || raw.running === 'true',
    zygiskCompat: raw.zygiskCompat === true || raw.zygiskCompat === 'true',
  };
}

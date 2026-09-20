import { apiBinary, execJson } from './ksu';

export interface InjectorStatus {
  running: boolean;
  pid: number;
  mode?: string;
}

export async function getInjectorStatus(): Promise<InjectorStatus> {
  const raw = await execJson<{
    running?: unknown;
    pid?: number;
    mode?: string;
  }>(`'${apiBinary()}' --ctl status`);
  return {
    pid: typeof raw.pid === 'number' ? raw.pid : 0,
    mode: typeof raw.mode === 'string' ? raw.mode : undefined,
    running: raw.running === true || raw.running === 'true',
  };
}

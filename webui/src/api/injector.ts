import { apiBinary, execJson } from './ksu';

export interface InjectorStatus {
  running: boolean;
  pid: number;
  zygiskCompat: boolean;
  mode?: string;
}

export async function getInjectorStatus(): Promise<InjectorStatus> {
  return execJson<InjectorStatus>(`'${apiBinary()}' --ctl status`);
}

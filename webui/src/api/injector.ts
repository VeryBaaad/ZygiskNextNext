import { apiScript, execJson } from './ksu';

export interface InjectorStatus {
  running: boolean;
  pid: number;
  zygiskCompat: boolean;
}

export async function getInjectorStatus(): Promise<InjectorStatus> {
  return execJson<InjectorStatus>(`sh '${apiScript()}' status`);
}

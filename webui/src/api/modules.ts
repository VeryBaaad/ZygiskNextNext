import { apiBinary, execJson } from './ksu';

export interface InjectedProcess {
  pid: number;
  name: string;
}

export interface FailedProcess {
  name: string;
  reason: string;
}

export interface ZnnModule {
  id: string;
  name: string;
  version: string;
  processes: InjectedProcess[];
  failed?: FailedProcess[];
}

export async function getModules(): Promise<ZnnModule[]> {
  return execJson<ZnnModule[]>(`'${apiBinary()}' --ctl modules`);
}

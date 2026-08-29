import { apiScript, execJson } from './ksu';

export interface InjectedProcess {
  pid: number;
  name: string;
}

export interface ZnnModule {
  id: string;
  name: string;
  version: string;
  processes: InjectedProcess[];
}

export async function getModules(): Promise<ZnnModule[]> {
  return execJson<ZnnModule[]>(`sh '${apiScript()}' modules`);
}

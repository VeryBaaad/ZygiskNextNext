import { apiBinary, execJson } from './ksu';

export interface HookEngineEntry {
  value: string;
  options: string[];
}

export interface HookConfig {
  inlineHook: HookEngineEntry;
  pltHook: HookEngineEntry;
  mode: HookEngineEntry;
}

export type HookKind = 'inline' | 'plt' | 'mode';

export async function getHookConfig(): Promise<HookConfig> {
  return execJson<HookConfig>(`'${apiBinary()}' --ctl config`);
}

export async function setHookConfig(kind: HookKind, value: string): Promise<HookConfig> {
  return execJson<HookConfig>(`'${apiBinary()}' --ctl config-set ${kind} ${value}`);
}

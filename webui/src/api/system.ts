import { apiScript, execJson } from './ksu';

export interface RootImplementation {
  magisk: string | null;
  kernelSU: string | null;
  apatch: string | null;
}

export interface SystemInfo {
  kernel: string;
  sdk: number;
  abi: string;
  abilist: string;
  root: RootImplementation;
}

export async function getSystemInfo(): Promise<SystemInfo> {
  return execJson<SystemInfo>(`sh '${apiScript()}' system`);
}

export function cleanVersion(raw: string | null): string | null {
  if (!raw) return null;
  return raw.replace(/^(ksud|apd|magisk)\s+/i, '').trim() || null;
}

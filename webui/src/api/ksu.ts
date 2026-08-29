import { exec, moduleInfo } from 'kernelsu';
import { MODULE_ID } from '../app-info';

export function isKsuAvailable(): boolean {
  return typeof (window as unknown as { ksu?: unknown }).ksu !== 'undefined';
}

export async function execJson<T>(command: string): Promise<T> {
  const { errno, stdout, stderr } = await exec(command);
  if (errno !== 0) {
    throw new Error(`command failed (errno ${errno}): ${command}\n${stderr}`);
  }
  return JSON.parse(stdout.trim()) as T;
}

export function getModuleDir(): string {
  try {
    const info = JSON.parse(moduleInfo()) as { moduleDir?: string };
    if (info && typeof info.moduleDir === 'string' && info.moduleDir.length > 0) {
      return info.moduleDir;
    }
  } catch {
  }
  return `/data/adb/modules/${MODULE_ID}`;
}

export function apiScript(): string {
  return `${getModuleDir()}/webroot/znn_api.sh`;
}

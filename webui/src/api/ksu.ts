import { exec, moduleInfo } from 'kernelsu';
import { MODULE_ID } from '../app-info';

export function isKsuAvailable(): boolean {
  return typeof (window as unknown as { ksu?: unknown }).ksu !== 'undefined';
}

export async function execJson<T>(command: string, timeoutMs = 5000): Promise<T> {
  const result = await Promise.race([
    exec(command),
    new Promise<never>((_, reject) =>
      setTimeout(() => reject(new Error(`command timed out: ${command}`)), timeoutMs),
    ),
  ]);
  if (result.errno !== 0) {
    throw new Error(`command failed (errno ${result.errno}): ${command}\n${result.stderr}`);
  }
  return JSON.parse(result.stdout.trim()) as T;
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

/**
 * The injector daemon doubles as the WebUI's control client: it maintains the
 * runtime state (what it scanned, which processes it injected into) in
 * /data/adb/zygisksu/znn_state.json, and `injector --ctl <cmd>` echoes that
 * snapshot as JSON. No shell script is involved, so a WebUI load is a couple
 * of fast native process spawns instead of a /proc-wide shell scan.
 */
export function apiBinary(): string {
  return `${getModuleDir()}/bin/injector`;
}

/*
 * This file is part of Zygisk Next Next.
 *
 * Zygisk Next Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zygisk Next Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zygisk Next Next. If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2026 VeryBaaad <verybaaad@outlook.com>
 */

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

export function apiBinary(): string {
  return `${getModuleDir()}/bin/injector`;
}

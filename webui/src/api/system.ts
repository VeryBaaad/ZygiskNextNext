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

import { apiBinary, execJson } from './ksu';

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
  return execJson<SystemInfo>(`'${apiBinary()}' --ctl system`);
}

export function cleanVersion(raw: string | null): string | null {
  if (!raw) return null;
  return raw.replace(/^(ksud|apd|magisk)\s+/i, '').replace(/^v/i, '').trim() || null;
}

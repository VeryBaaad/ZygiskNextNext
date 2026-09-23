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

export interface InjectorStatus {
  running: boolean;
  pid: number;
  mode?: string;
}

export async function getInjectorStatus(): Promise<InjectorStatus> {
  const raw = await execJson<{
    running?: unknown;
    pid?: number;
    mode?: string;
  }>(`'${apiBinary()}' --ctl status`);
  return {
    pid: typeof raw.pid === 'number' ? raw.pid : 0,
    mode: typeof raw.mode === 'string' ? raw.mode : undefined,
    running: raw.running === true || raw.running === 'true',
  };
}

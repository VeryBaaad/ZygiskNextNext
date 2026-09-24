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

import { ref, type Ref } from 'vue';

import { getHookConfig, setHookConfig, type HookConfig, type HookKind } from '../api/config';
import { getInjectorStatus, type InjectorStatus } from '../api/injector';
import { isKsuAvailable } from '../api/ksu';
import { getModules, type ZnnModule } from '../api/modules';
import { getSystemInfo, type SystemInfo } from '../api/system';

export interface AppData {
  ksuAvailable: boolean;
  status: Ref<InjectorStatus | null>;
  system: Ref<SystemInfo | null>;
  modules: Ref<ZnnModule[] | null>;
  config: Ref<HookConfig | null>;
  loading: Ref<boolean>;
  load: () => Promise<void>;
  applyHookConfig: (kind: HookKind, value: string) => Promise<void>;
}

export function useAppData(): AppData {
  const ksuAvailable = isKsuAvailable();
  const status = ref<InjectorStatus | null>(null);
  const system = ref<SystemInfo | null>(null);
  const modules = ref<ZnnModule[] | null>(null);
  const config = ref<HookConfig | null>(null);
  const loading = ref(false);

  async function load(): Promise<void> {
    if (!ksuAvailable || loading.value) return;
    loading.value = true;
    try {
      const [nextStatus, nextSystem, nextModules, nextConfig] = await Promise.all([
        getInjectorStatus().catch((e) => {
          console.warn('[znn] status:', e);
          return null;
        }),
        getSystemInfo().catch((e) => {
          console.warn('[znn] system:', e);
          return null;
        }),
        getModules().catch((e) => {
          console.warn('[znn] modules:', e);
          return null;
        }),
        getHookConfig().catch((e) => {
          console.warn('[znn] config:', e);
          return null;
        }),
      ]);
      status.value = nextStatus;
      system.value = nextSystem;
      modules.value = nextModules;
      config.value = nextConfig;
    } finally {
      loading.value = false;
    }
  }

  async function applyHookConfig(kind: HookKind, value: string): Promise<void> {
    config.value = await setHookConfig(kind, value);
  }

  return { ksuAvailable, status, system, modules, config, loading, load, applyHookConfig };
}

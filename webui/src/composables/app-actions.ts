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

import { inject, provide, type InjectionKey } from 'vue';

import type { HookKind } from '../api/config';

export interface AppActions {
  refresh: () => void;
  applyHookConfig: (kind: HookKind, value: string) => Promise<void>;
}

const APP_ACTIONS: InjectionKey<AppActions> = Symbol('znn-app-actions');

export function provideAppActions(actions: AppActions): void {
  provide(APP_ACTIONS, actions);
}

export function useAppActions(): AppActions {
  const actions = inject(APP_ACTIONS);
  if (!actions) throw new Error('app actions were not provided');
  return actions;
}

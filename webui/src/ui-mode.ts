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

import { computed, ref, watch, type ComputedRef } from 'vue';

export type UiMode = 'material' | 'miuix' | 'zn';

export const UI_MODE_STORAGE_KEY = 'znn_ui';

export const DEFAULT_UI_MODE: UiMode = 'material';

export const UI_MODES: UiMode[] = ['material', 'miuix', 'zn'];

const modeRef = ref<UiMode>(detectUiMode());

export const uiMode: ComputedRef<UiMode> = computed(() => modeRef.value);

function detectUiMode(): UiMode {
  try {
    const saved = localStorage.getItem(UI_MODE_STORAGE_KEY);
    if (saved && isUiMode(saved)) return saved;
  } catch {
  }
  return DEFAULT_UI_MODE;
}

export function isUiMode(value: string): value is UiMode {
  return (UI_MODES as string[]).includes(value);
}

export function setUiMode(next: UiMode): void {
  if (next === modeRef.value) return;
  modeRef.value = next;
  try {
    localStorage.setItem(UI_MODE_STORAGE_KEY, next);
  } catch {
  }
}

function apply(): void {
  document.documentElement.dataset.znnUi = modeRef.value;
}

export function initUiMode(): void {
  watch(modeRef, apply, { immediate: true });
}

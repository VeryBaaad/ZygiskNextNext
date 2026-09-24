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

export type ThemeMode = 'auto' | 'light' | 'dark';

export type ThemeScheme = 'light' | 'dark';

export const THEME_STORAGE_KEY = 'znn_theme';

export const THEME_MODES: ThemeMode[] = ['auto', 'light', 'dark'];

const MIUIX_DARK_CLASS = 'm-theme-dark';

const HOST_ID = 'znn-theme';

const media = window.matchMedia('(prefers-color-scheme: dark)');

const modeRef = ref<ThemeMode>(detectTheme());

const systemDarkRef = ref(media.matches);

export const themeMode: ComputedRef<ThemeMode> = computed(() => modeRef.value);

export const themeScheme: ComputedRef<ThemeScheme> = computed(() =>
  modeRef.value === 'auto' ? (systemDarkRef.value ? 'dark' : 'light') : modeRef.value,
);

function detectTheme(): ThemeMode {
  try {
    const saved = localStorage.getItem(THEME_STORAGE_KEY);
    if (saved === 'light' || saved === 'dark' || saved === 'auto') {
      return saved;
    }
  } catch {
  }
  return 'auto';
}

export function setThemeMode(next: ThemeMode): void {
  if (next === modeRef.value) return;
  modeRef.value = next;
  try {
    localStorage.setItem(THEME_STORAGE_KEY, next);
  } catch {
  }
}

function apply(scheme: ThemeScheme): void {
  const host = document.getElementById(HOST_ID) as (HTMLElement & { scheme?: ThemeScheme }) | null;
  if (host) host.scheme = scheme;
  document.documentElement.style.colorScheme = scheme;
  document.documentElement.dataset.znnScheme = scheme;
  document.documentElement.classList.toggle(MIUIX_DARK_CLASS, scheme === 'dark');
}

export function initTheme(): void {
  media.addEventListener('change', (event) => {
    systemDarkRef.value = event.matches;
  });
  watch(themeScheme, apply, { immediate: true });
}

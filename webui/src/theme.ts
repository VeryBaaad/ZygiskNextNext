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

export type ThemeMode = 'auto' | 'light' | 'dark';

export type ThemeScheme = 'light' | 'dark';

export const THEME_STORAGE_KEY = 'znn_theme';

const HOST_ID = 'znn-theme';

const media = window.matchMedia('(prefers-color-scheme: dark)');

const listeners = new Set<() => void>();

let mode: ThemeMode = detectTheme();
let mediaListener: (() => void) | null = null;

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

function host(): HTMLElement & { scheme?: ThemeScheme } {
  return document.getElementById(HOST_ID) as HTMLElement & { scheme?: ThemeScheme };
}

export function getThemeMode(): ThemeMode {
  return mode;
}

function getThemeScheme(): ThemeScheme {
  if (mode !== 'auto') return mode;
  return media.matches ? 'dark' : 'light';
}

function apply(): void {
  const el = host();
  if (!el) return;
  el.scheme = getThemeScheme();
}

export function setThemeMode(next: ThemeMode): void {
  if (next === mode) return;
  mode = next;
  try {
    localStorage.setItem(THEME_STORAGE_KEY, next);
  } catch {
  }
  apply();
  for (const fn of listeners) fn();
}

export function onThemeChange(fn: () => void): () => void {
  listeners.add(fn);
  return () => listeners.delete(fn);
}

export function initTheme(): void {
  if (!mediaListener) {
    mediaListener = () => {
      if (mode === 'auto') {
        apply();
        for (const fn of listeners) fn();
      }
    };
    media.addEventListener('change', mediaListener);
  }
  apply();
}

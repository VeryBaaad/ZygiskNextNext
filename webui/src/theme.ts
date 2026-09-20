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

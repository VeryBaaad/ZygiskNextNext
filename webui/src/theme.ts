export type ThemeMode = 'auto' | 'light' | 'dark';

export const THEME_STORAGE_KEY = 'znn_theme';

const media = window.matchMedia('(prefers-color-scheme: dark)');

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

export function getThemeMode(): ThemeMode {
  return mode;
}

function resolvedDark(): boolean {
  return mode === 'dark' || (mode === 'auto' && media.matches);
}

function apply(): void {
  document.documentElement.dataset.theme = resolvedDark() ? 'dark' : 'light';
}

export function setThemeMode(next: ThemeMode): void {
  mode = next;
  try {
    localStorage.setItem(THEME_STORAGE_KEY, next);
  } catch {
  }
  apply();
}

export function cycleTheme(): ThemeMode {
  const order: ThemeMode[] = ['auto', 'light', 'dark'];
  const next = order[(order.indexOf(mode) + 1) % order.length];
  setThemeMode(next);
  return next;
}

export function initTheme(): void {
  if (!mediaListener) {
    mediaListener = () => {
      if (mode === 'auto') apply();
    };
    media.addEventListener('change', mediaListener);
  }
  apply();
}

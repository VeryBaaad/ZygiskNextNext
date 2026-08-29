import { en, type Dictionary } from './en';
import { zhCN } from './zh-CN';

export type Locale = 'zh-CN' | 'en';

export const LOCALE_STORAGE_KEY = 'znn_locale';

const dictionaries: Record<Locale, Dictionary> = {
  'zh-CN': zhCN,
  en,
};

export const LOCALE_LABELS: Record<Locale, string> = {
  'zh-CN': '简体中文',
  en: 'English',
};

let current: Locale = detectLocale();
let strings: Dictionary = dictionaries[current];

const listeners = new Set<() => void>();

function detectLocale(): Locale {
  try {
    const saved = localStorage.getItem(LOCALE_STORAGE_KEY);
    if (saved === 'zh-CN' || saved === 'en') return saved;
  } catch {
  }
  const lang = (navigator.language || 'en').toLowerCase();
  return lang.startsWith('zh') ? 'zh-CN' : 'en';
}

export function getLocale(): Locale {
  return current;
}

export function setLocale(locale: Locale): void {
  if (locale === current) return;
  current = locale;
  strings = dictionaries[locale];
  try {
    localStorage.setItem(LOCALE_STORAGE_KEY, locale);
  } catch {
  }
  document.documentElement.lang = locale;
  for (const fn of listeners) fn();
}

export function t(
  key: keyof Dictionary | string,
  params?: Record<string, string | number>,
): string {
  let text: string =
    (strings as Record<string, string>)[key] ??
    (dictionaries.en as Record<string, string>)[key] ??
    key;
  if (params) {
    for (const [k, v] of Object.entries(params)) {
      text = text.split(`{${k}}`).join(String(v));
    }
  }
  return text;
}

export function onLocaleChange(fn: () => void): () => void {
  listeners.add(fn);
  return () => listeners.delete(fn);
}

export function applyLocale(): void {
  document.documentElement.lang = current;
  for (const fn of listeners) fn();
}

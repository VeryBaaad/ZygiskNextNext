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

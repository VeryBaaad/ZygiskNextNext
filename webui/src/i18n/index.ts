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

import { computed, ref, type ComputedRef } from 'vue';

import { en, type Dictionary } from './en';
import { zhCN } from './zh-CN';

export type Locale = 'zh-CN' | 'en';

export const LOCALE_STORAGE_KEY = 'znn_locale';

export const LOCALES: Locale[] = ['zh-CN', 'en'];

const dictionaries: Record<Locale, Dictionary> = {
  'zh-CN': zhCN,
  en,
};

export const LOCALE_LABELS: Record<Locale, string> = {
  'zh-CN': '简体中文',
  en: 'English',
};

const currentRef = ref<Locale>(detectLocale());

export const locale: ComputedRef<Locale> = computed(() => currentRef.value);

function detectLocale(): Locale {
  try {
    const saved = localStorage.getItem(LOCALE_STORAGE_KEY);
    if (saved === 'zh-CN' || saved === 'en') return saved;
  } catch {
  }
  const lang = (navigator.language || 'en').toLowerCase();
  return lang.startsWith('zh') ? 'zh-CN' : 'en';
}

export function setLocale(next: Locale): void {
  if (next === currentRef.value) return;
  currentRef.value = next;
  try {
    localStorage.setItem(LOCALE_STORAGE_KEY, next);
  } catch {
  }
  applyLocale();
}

export function t(key: string, params?: Record<string, string | number>): string {
  const strings = dictionaries[currentRef.value] as Record<string, string>;
  let text: string = strings[key] ?? (en as Record<string, string>)[key] ?? key;
  if (params) {
    for (const [k, v] of Object.entries(params)) {
      text = text.split(`{${k}}`).join(String(v));
    }
  }
  return text;
}

export function applyLocale(): void {
  document.documentElement.lang = currentRef.value;
}

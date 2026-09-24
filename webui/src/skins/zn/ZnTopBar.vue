<script setup lang="ts">
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

import { useTemplateRef } from 'vue';

import { MODULE_NAME } from '../../app-info';
import { LOCALE_LABELS, LOCALES, locale, setLocale, t, type Locale } from '../../i18n';
import { THEME_MODES, setThemeMode, themeMode, type ThemeMode } from '../../theme';
import ZnIcon from './ZnIcon.vue';
import type { IconName } from './icons';
import { usePopover } from './use-popover';

const THEME_ICONS: Record<ThemeMode, IconName> = {
  auto: 'themeAuto',
  light: 'themeLight',
  dark: 'themeDark',
};

const THEME_LABEL_KEYS: Record<ThemeMode, string> = {
  auto: 'theme.auto',
  light: 'theme.light',
  dark: 'theme.dark',
};

const themeRoot = useTemplateRef<HTMLElement>('themeRoot');
const localeRoot = useTemplateRef<HTMLElement>('localeRoot');

const {
  open: themeOpen,
  above: themeAbove,
  toggle: toggleThemeMenu,
  close: closeThemeMenu,
} = usePopover(() => themeRoot.value);

const {
  open: localeOpen,
  above: localeAbove,
  toggle: toggleLocaleMenu,
  close: closeLocaleMenu,
} = usePopover(() => localeRoot.value);

function pickTheme(mode: ThemeMode): void {
  closeThemeMenu();
  setThemeMode(mode);
}

function pickLocale(next: Locale): void {
  closeLocaleMenu();
  setLocale(next);
}
</script>

<template>
  <header class="zn-page-header">
    <h1 class="zn-page-title">{{ MODULE_NAME }}</h1>

    <div class="zn-page-actions">
      <div ref="themeRoot" class="zn-select-wrap">
        <button
          class="zn-icon-button"
          type="button"
          :aria-label="t('topbar.themeLabel')"
          :aria-expanded="themeOpen"
          aria-haspopup="menu"
          @click="toggleThemeMenu()"
        >
          <ZnIcon :name="THEME_ICONS[themeMode]" :size="20" />
        </button>
        <div v-if="themeOpen" class="zn-menu" :class="{ 'is-above': themeAbove }" role="menu">
          <button
            v-for="mode in THEME_MODES"
            :key="mode"
            class="zn-menu-item"
            :class="{ 'is-selected': mode === themeMode }"
            type="button"
            role="menuitemradio"
            :aria-checked="mode === themeMode"
            @click="pickTheme(mode)"
          >
            <span>{{ t(THEME_LABEL_KEYS[mode]) }}</span>
            <ZnIcon class="zn-menu-check" name="check" :size="16" />
          </button>
        </div>
      </div>

      <div ref="localeRoot" class="zn-select-wrap">
        <button
          class="zn-icon-button"
          type="button"
          :aria-label="t('topbar.langLabel')"
          :aria-expanded="localeOpen"
          aria-haspopup="menu"
          @click="toggleLocaleMenu()"
        >
          <ZnIcon name="language" :size="20" />
        </button>
        <div v-if="localeOpen" class="zn-menu" :class="{ 'is-above': localeAbove }" role="menu">
          <button
            v-for="item in LOCALES"
            :key="item"
            class="zn-menu-item"
            :class="{ 'is-selected': item === locale }"
            type="button"
            role="menuitemradio"
            :aria-checked="item === locale"
            @click="pickLocale(item)"
          >
            <span>{{ LOCALE_LABELS[item] }}</span>
            <ZnIcon class="zn-menu-check" name="check" :size="16" />
          </button>
        </div>
      </div>
    </div>
  </header>
</template>

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

import { onBeforeUnmount, onMounted, ref } from 'vue';

import { MODULE_ID, MODULE_NAME } from '../../app-info';
import { useAppActions } from '../../composables/app-actions';
import { LOCALE_LABELS, LOCALES, locale, setLocale, t } from '../../i18n';
import { THEME_MODES, setThemeMode, themeMode, type ThemeMode } from '../../theme';
import { cubicBezier } from '../../util/easing';

const props = defineProps<{
  ksuAvailable: boolean;
  refreshing: boolean;
}>();

const THEME_ICONS: Record<ThemeMode, string> = {
  auto: 'brightness_auto',
  light: 'light_mode',
  dark: 'dark_mode',
};

const THEME_LABEL_KEYS: Record<ThemeMode, string> = {
  auto: 'theme.auto',
  light: 'theme.light',
  dark: 'theme.dark',
};

const PROGRESS_VAR = '--znn-bar-progress';

const TITLE_ALPHA_VAR = '--znn-bar-title-alpha';

const EXPANDED_VAR = '--znn-bar-expanded';

const COLLAPSED_VAR = '--znn-bar-collapsed';

const COLLAPSED_TITLE_EASING = cubicBezier(0.8, 0, 0.8, 0.15);

const actions = useAppActions();

const host = ref<HTMLElement | null>(null);

let frame = 0;
let collapseSpan = 0;

function measureCollapseSpan(): void {
  const el = host.value;
  if (!el) return;
  const styles = getComputedStyle(el);
  const expanded = parseFloat(styles.getPropertyValue(EXPANDED_VAR));
  const collapsed = parseFloat(styles.getPropertyValue(COLLAPSED_VAR));
  collapseSpan = expanded > collapsed ? expanded - collapsed : 1;
}

function scheduleUpdate(): void {
  if (frame) return;
  frame = requestAnimationFrame(() => {
    frame = 0;
    updateScrollState();
  });
}

function updateScrollState(): void {
  const el = host.value;
  if (!el) return;
  if (collapseSpan <= 0) measureCollapseSpan();
  const scrolled = window.scrollY || document.documentElement.scrollTop || 0;
  const progress = Math.min(1, Math.max(0, scrolled / collapseSpan));
  el.style.setProperty(PROGRESS_VAR, progress.toFixed(4));
  el.style.setProperty(TITLE_ALPHA_VAR, COLLAPSED_TITLE_EASING(progress).toFixed(4));
}

function onResize(): void {
  measureCollapseSpan();
  scheduleUpdate();
}

onMounted(() => {
  window.addEventListener('scroll', scheduleUpdate, { passive: true });
  window.addEventListener('resize', onResize, { passive: true });
  scheduleUpdate();
});

onBeforeUnmount(() => {
  window.removeEventListener('scroll', scheduleUpdate);
  window.removeEventListener('resize', onResize);
  if (frame) cancelAnimationFrame(frame);
  frame = 0;
});
</script>

<template>
  <div ref="host" class="top-bar-host">
    <header class="top-bar">
      <div class="top-bar-row">
        <h1 class="top-bar-title" :title="MODULE_ID">{{ MODULE_NAME }}</h1>
        <div class="top-bar-actions">
          <m3e-icon-button
            v-if="props.ksuAvailable"
            class="top-bar-refresh"
            :class="{ 'is-loading': props.refreshing }"
            :aria-label="t('actions.refresh')"
            :disabled="props.refreshing"
            @click="actions.refresh()"
          >
            <m3e-icon name="refresh"></m3e-icon>
          </m3e-icon-button>
          <m3e-icon-button :aria-label="t('topbar.themeLabel')">
            <m3e-menu-trigger for="znn-theme-menu">
              <m3e-icon :name="THEME_ICONS[themeMode]"></m3e-icon>
            </m3e-menu-trigger>
          </m3e-icon-button>
          <m3e-icon-button :aria-label="t('topbar.langLabel')">
            <m3e-menu-trigger for="znn-locale-menu">
              <m3e-icon name="language"></m3e-icon>
            </m3e-menu-trigger>
          </m3e-icon-button>
        </div>
      </div>
      <div class="top-bar-expanded">
        <span class="top-bar-title top-bar-expanded-title" aria-hidden="true">
          {{ MODULE_NAME }}
        </span>
      </div>
    </header>

    <m3e-menu id="znn-theme-menu" position-x="before">
      <m3e-menu-item-radio
        v-for="mode in THEME_MODES"
        :key="mode"
        :checked="mode === themeMode"
        @click="setThemeMode(mode)"
      >
        {{ t(THEME_LABEL_KEYS[mode]) }}
      </m3e-menu-item-radio>
    </m3e-menu>

    <m3e-menu id="znn-locale-menu" position-x="before">
      <m3e-menu-item-radio
        v-for="item in LOCALES"
        :key="item"
        :checked="item === locale"
        @click="setLocale(item)"
      >
        {{ LOCALE_LABELS[item] }}
      </m3e-menu-item-radio>
    </m3e-menu>
  </div>
</template>

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

import {
  MiuixBottomSheet,
  MiuixIcon,
  MiuixIconButton,
  MiuixRadioButtonPreference,
  MiuixTopAppBar,
} from 'miuix-vue';
import { ref } from 'vue';

import { MODULE_NAME } from '../../app-info';
import { useAppActions } from '../../composables/app-actions';
import { LOCALE_LABELS, LOCALES, locale, setLocale, t } from '../../i18n';
import { THEME_MODES, setThemeMode, themeMode, type ThemeMode } from '../../theme';
import { Refresh, Theme, Translate } from './icons';

const props = defineProps<{
  ksuAvailable: boolean;
  refreshing: boolean;
}>();

const THEME_LABEL_KEYS: Record<ThemeMode, string> = {
  auto: 'theme.auto',
  light: 'theme.light',
  dark: 'theme.dark',
};

const actions = useAppActions();

const themeSheet = ref(false);
const localeSheet = ref(false);

function pickTheme(mode: ThemeMode): void {
  setThemeMode(mode);
  themeSheet.value = false;
}

function pickLocale(next: (typeof LOCALES)[number]): void {
  setLocale(next);
  localeSheet.value = false;
}
</script>

<template>
  <MiuixTopAppBar class="miuix-app-bar" :title="MODULE_NAME">
    <template #actions>
      <MiuixIconButton
        v-if="props.ksuAvailable"
        :aria-label="t('actions.refresh')"
        :disabled="props.refreshing"
        @click="actions.refresh()"
      >
        <MiuixIcon :icon="Refresh" :size="24" />
      </MiuixIconButton>
      <MiuixIconButton :aria-label="t('topbar.themeLabel')" @click="themeSheet = true">
        <MiuixIcon :icon="Theme" :size="24" />
      </MiuixIconButton>
      <MiuixIconButton :aria-label="t('topbar.langLabel')" @click="localeSheet = true">
        <MiuixIcon :icon="Translate" :size="24" />
      </MiuixIconButton>
    </template>
  </MiuixTopAppBar>

  <MiuixBottomSheet v-model="themeSheet" :title="t('topbar.themeLabel')">
    <MiuixRadioButtonPreference
      v-for="mode in THEME_MODES"
      :key="mode"
      :model-value="mode === themeMode"
      :title="t(THEME_LABEL_KEYS[mode])"
      location="end"
      @select="pickTheme(mode)"
    />
  </MiuixBottomSheet>

  <MiuixBottomSheet v-model="localeSheet" :title="t('topbar.langLabel')">
    <MiuixRadioButtonPreference
      v-for="item in LOCALES"
      :key="item"
      :model-value="item === locale"
      :title="LOCALE_LABELS[item]"
      location="end"
      @select="pickLocale(item)"
    />
  </MiuixBottomSheet>
</template>

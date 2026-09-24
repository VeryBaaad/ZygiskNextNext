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

import { computed, defineAsyncComponent, onMounted } from 'vue';

import { provideAppActions } from './composables/app-actions';
import { useAppData } from './composables/use-app-data';
import { applyLocale } from './i18n';
import { initTheme } from './theme';
import { initUiMode, uiMode } from './ui-mode';

const MaterialShell = defineAsyncComponent(() => import('./skins/material/MaterialShell.vue'));
const MiuixShell = defineAsyncComponent(() => import('./skins/miuix/MiuixShell.vue'));

const { ksuAvailable, loading, status, system, modules, config, load, applyHookConfig } =
  useAppData();

provideAppActions({
  refresh: () => {
    void load();
  },
  applyHookConfig: (kind, value) => applyHookConfig(kind, value),
});

const shell = computed(() => (uiMode.value === 'miuix' ? MiuixShell : MaterialShell));

initTheme();
initUiMode();
applyLocale();

onMounted(() => {
  void load();
});
</script>

<template>
  <component
    :is="shell"
    :key="uiMode"
    :ksu-available="ksuAvailable"
    :loading="loading"
    :status="status"
    :system="system"
    :modules="modules"
    :config="config"
  />
</template>

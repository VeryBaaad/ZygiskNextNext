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
  MiuixBasicComponent,
  MiuixBottomSheet,
  MiuixCard,
  MiuixIcon,
  MiuixSmallTitle,
} from 'miuix-vue';
import { computed, ref } from 'vue';

import type { ZnnModule } from '../../api/modules';
import { t } from '../../i18n';
import { ChevronForward } from './icons';

const props = defineProps<{ modules: ZnnModule[] }>();

const selected = ref<ZnnModule | null>(null);

const sheetOpen = ref(false);

const processes = computed(() => selected.value?.processes ?? []);

const failures = computed(() => selected.value?.failed ?? []);

const sheetTitle = computed(() =>
  selected.value ? `${selected.value.name} (${selected.value.id})` : '',
);

const basename = (value: string): string => {
  const cut = value.lastIndexOf('/');
  return cut >= 0 ? value.slice(cut + 1) : value;
};

function summary(module: ZnnModule): string {
  const parts = [t('modules.processes', { n: module.processes.length })];
  const failed = (module.failed ?? []).length;
  if (failed > 0) parts.push(t('modules.failed', { n: failed }));
  return parts.join(' · ');
}

function open(module: ZnnModule): void {
  selected.value = module;
  sheetOpen.value = true;
}
</script>

<template>
  <MiuixCard class="miuix-modules-card">
    <MiuixBasicComponent
      v-if="props.modules.length === 0"
      :title="t('modules.noModules')"
      disabled
    />
    <MiuixBasicComponent
      v-for="module in props.modules"
      :key="module.id"
      class="miuix-module-row"
      clickable
      :title="`${module.name} (${module.id})`"
      :summary="summary(module)"
      @click="open(module)"
    >
      <template #end>
        <span class="miuix-chevron"><MiuixIcon :icon="ChevronForward" :size="20" /></span>
      </template>
    </MiuixBasicComponent>
  </MiuixCard>

  <MiuixBottomSheet v-model="sheetOpen" :title="sheetTitle">
    <MiuixBasicComponent
      v-if="processes.length === 0 && failures.length === 0"
      :title="t('modules.noProcesses')"
      disabled
    />
    <MiuixBasicComponent
      v-for="process in processes"
      :key="process.pid"
      :title="basename(process.name)"
      :summary="`pid=${process.pid}`"
    />
    <template v-if="failures.length > 0">
      <MiuixSmallTitle class="miuix-sheet-section" :text="t('modules.failedTitle')" />
      <MiuixBasicComponent
        v-for="failure in failures"
        :key="`${failure.name}-${failure.reason}`"
        :title="basename(failure.name)"
        :summary="failure.reason"
        title-color="var(--m-color-error)"
      />
    </template>
  </MiuixBottomSheet>
</template>

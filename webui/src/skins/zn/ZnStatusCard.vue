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

import { computed } from 'vue';

import type { InjectorStatus } from '../../api/injector';
import type { ZnnModule } from '../../api/modules';
import { cleanVersion, type RootImplementation } from '../../api/system';
import { useAppActions } from '../../composables/app-actions';
import { t } from '../../i18n';
import ZnCard from './ZnCard.vue';
import ZnCollapse from './ZnCollapse.vue';
import ZnIcon from './ZnIcon.vue';
import ZnModuleList from './ZnModuleList.vue';
import ZnTag from './ZnTag.vue';

const props = defineProps<{
  status: InjectorStatus | null;
  root: RootImplementation | null;
  modules: ZnnModule[];
  refreshing: boolean;
  error?: boolean;
}>();

const actions = useAppActions();

const rootLabel = computed(() => {
  const impl = props.root;
  if (!impl) return t('info.rootImpl.none');
  const entries = [
    { name: 'KernelSU', version: cleanVersion(impl.kernelSU) },
    { name: 'Magisk', version: cleanVersion(impl.magisk) },
    { name: 'APatch', version: cleanVersion(impl.apatch) },
  ].filter((entry) => entry.version !== null);
  if (entries.length === 0) return t('info.rootImpl.none');
  return entries.map((entry) => `${entry.name} (${entry.version})`).join(', ');
});

const injectorLabel = computed(() => {
  if (!props.status?.running) return t('zn.stopped');
  return props.status.pid
    ? `${t('zn.running')} (${props.status.pid})`
    : t('zn.running');
});
</script>

<template>
  <ZnCard :title="t('card.status')" :error="props.error">
    <template #extra>
      <button
        class="zn-icon-button"
        type="button"
        :class="{ 'is-busy': props.refreshing }"
        :aria-label="t('actions.refresh')"
        :disabled="props.refreshing"
        @click="actions.refresh()"
      >
        <ZnIcon name="refresh" :size="20" />
      </button>
    </template>

    <div class="zn-rows">
      <div class="zn-row">
        <span class="zn-field-label">{{ t('info.rootImpl') }}</span>
        <ZnTag :tone="props.root ? 'ok' : 'warn'">{{ rootLabel }}</ZnTag>
      </div>

      <div class="zn-row">
        <span class="zn-field-label">{{ t('zn.injector') }}</span>
        <ZnTag :tone="props.status?.running ? 'ok' : 'warn'">{{ injectorLabel }}</ZnTag>
      </div>
    </div>

    <ZnCollapse :title="t('zn.modules.title', { n: props.modules.length })">
      <ZnModuleList :modules="props.modules" />
    </ZnCollapse>
  </ZnCard>
</template>

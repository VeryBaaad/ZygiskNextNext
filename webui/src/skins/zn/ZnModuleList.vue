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

import { ref } from 'vue';

import type { ZnnModule } from '../../api/modules';
import { t } from '../../i18n';
import ZnTag from './ZnTag.vue';

const props = defineProps<{ modules: ZnnModule[] }>();

const expanded = ref(new Set<string>());

function toggle(id: string): void {
  const next = new Set(expanded.value);
  if (next.has(id)) next.delete(id);
  else next.add(id);
  expanded.value = next;
}

function failures(module: ZnnModule): number {
  return (module.failed ?? []).length;
}

const basename = (value: string): string => {
  const cut = value.lastIndexOf('/');
  return cut >= 0 ? value.slice(cut + 1) : value;
};
</script>

<template>
  <div v-if="props.modules.length === 0" class="zn-empty-line">{{ t('zn.modules.empty') }}</div>

  <div v-else class="zn-module-list">
    <div v-for="module in props.modules" :key="module.id" class="zn-module">
      <button
        class="zn-module-trigger"
        type="button"
        :aria-expanded="expanded.has(module.id)"
        @click="toggle(module.id)"
      >
        <span class="zn-module-name" :title="`${module.name} (${module.id})`">
          {{ module.name }}
        </span>
        <span class="zn-module-tags">
          <ZnTag v-if="failures(module) > 0" tone="warn" small>
            {{ t('zn.modules.badge') }}
          </ZnTag>
          <ZnTag tone="info" small>{{ t('modules.processes', { n: module.processes.length }) }}</ZnTag>
        </span>
      </button>

      <div v-if="expanded.has(module.id)" class="zn-module-body">
        <div v-if="module.processes.length === 0" class="zn-empty-line">
          {{ t('modules.noProcesses') }}
        </div>
        <div
          v-for="process in module.processes"
          :key="process.pid"
          class="zn-proc-row"
        >
          <span class="zn-proc-name" :title="process.name">{{ basename(process.name) }}</span>
          <span class="zn-proc-meta">pid={{ process.pid }}</span>
        </div>

        <template v-if="failures(module) > 0">
          <div class="zn-proc-section">{{ t('modules.failedTitle') }}</div>
          <div
            v-for="failure in module.failed ?? []"
            :key="`${failure.name}-${failure.reason}`"
            class="zn-proc-row zn-proc-failed"
          >
            <span class="zn-proc-name" :title="failure.name">{{ basename(failure.name) }}</span>
            <span class="zn-proc-meta" :title="failure.reason">{{ failure.reason }}</span>
          </div>
        </template>
      </div>
    </div>
  </div>
</template>

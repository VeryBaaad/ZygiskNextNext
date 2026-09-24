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

import type { ZnnModule } from '../../api/modules';
import { t } from '../../i18n';

const props = defineProps<{ modules: ZnnModule[] }>();

/** Expanded rows survive re-renders; the element owns the open state itself. */
const expanded = new Set<string>();

const basename = (value: string): string => {
  const cut = value.lastIndexOf('/');
  return cut >= 0 ? value.slice(cut + 1) : value;
};
</script>

<template>
  <m3e-card class="modules-card" variant="filled">
    <div slot="content" class="card-body">
      <m3e-list class="modules-list">
        <div v-if="props.modules.length === 0" class="modules-empty">
          {{ t('modules.noModules') }}
        </div>
        <m3e-expandable-list-item
          v-for="module in props.modules"
          :key="module.id"
          :open="expanded.has(module.id)"
          @opened="expanded.add(module.id)"
          @closed="expanded.delete(module.id)"
        >
          <span class="module-line">
            <span class="module-name" :title="`${module.name} (${module.id})`">
              {{ module.name }} ({{ module.id }})
            </span>
            <span
              class="module-count"
              :class="{ 'has-failed': (module.failed ?? []).length > 0 }"
            >
              {{ t('modules.processes', { n: module.processes.length }) }}
              <template v-if="(module.failed ?? []).length > 0">
                · {{ t('modules.failed', { n: (module.failed ?? []).length }) }}
              </template>
            </span>
          </span>
          <div slot="items" class="proc-list">
            <div v-if="module.processes.length === 0" class="proc-empty">—</div>
            <div v-for="process in module.processes" :key="process.pid" class="proc-row">
              <span class="proc-name" :title="basename(process.name)">
                {{ basename(process.name) }}
              </span>
              <span class="proc-pid">pid={{ process.pid }}</span>
            </div>
            <template v-if="(module.failed ?? []).length > 0">
              <div class="proc-failed-title">{{ t('modules.failedTitle') }}</div>
              <div
                v-for="failure in module.failed ?? []"
                :key="`${failure.name}-${failure.reason}`"
                class="proc-failed-row"
              >
                <span class="proc-name" :title="basename(failure.name)">
                  {{ basename(failure.name) }}
                </span>
                <span class="proc-failed-reason" :title="failure.reason">
                  {{ failure.reason }}
                </span>
              </div>
            </template>
          </div>
        </m3e-expandable-list-item>
      </m3e-list>
    </div>
  </m3e-card>
</template>

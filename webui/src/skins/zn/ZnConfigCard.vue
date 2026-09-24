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
 * Copyright (C) 2026 VeryBaaad <verybaad@outlook.com>
 */

import { computed, ref } from 'vue';

import type { HookConfig, HookKind } from '../../api/config';
import { useAppActions } from '../../composables/app-actions';
import { HOOK_ROWS, UI_ROW_LABEL_KEY, hookOptionLabel, uiModeLabel } from '../../config-model';
import { t } from '../../i18n';
import { UI_MODES, isUiMode, setUiMode, uiMode } from '../../ui-mode';
import ZnCard from './ZnCard.vue';
import ZnSelect from './ZnSelect.vue';

interface ConfigRow {
  kind: string;
  label: string;
  value: string;
  options: string[];
}

const props = defineProps<{ config: HookConfig; error?: boolean }>();

const actions = useAppActions();

/** Its own state so a failed write does not lock the whole card. */
const busy = ref(false);

const rows = computed<ConfigRow[]>(() => {
  const out: ConfigRow[] = HOOK_ROWS.map((row) => {
    const entry = props.config[row.field];
    return {
      kind: row.kind,
      label: t(row.labelKey),
      value: entry.value,
      options: [...entry.options],
    };
  });
  out.push({
    kind: 'ui',
    label: t(UI_ROW_LABEL_KEY),
    value: uiMode.value,
    options: [...UI_MODES],
  });
  return out;
});

function optionLabel(row: ConfigRow, id: string): string {
  if (row.kind === 'ui') return uiModeLabel(id);
  return hookOptionLabel(row.kind as HookKind, id);
}

async function select(row: ConfigRow, value: string): Promise<void> {
  if (value === row.value) return;
  if (row.kind === 'ui') {
    if (isUiMode(value)) setUiMode(value);
    return;
  }
  if (busy.value) return;
  busy.value = true;
  try {
    await actions.applyHookConfig(row.kind as HookKind, value);
  } catch (e) {
    console.warn('[znn] config-set:', e);
  } finally {
    busy.value = false;
  }
}
</script>

<template>
  <ZnCard :title="t('card.settings')" :error="props.error">
    <div v-if="!props.error" class="zn-rows">
      <div v-for="row in rows" :key="row.kind" class="zn-row">
        <span class="zn-field-label">{{ row.label }}</span>
        <ZnSelect
          :model-value="row.value"
          :options="row.options"
          :label="(id: string) => optionLabel(row, id)"
          :disabled="busy"
          @select="(id: string) => select(row, id)"
        />
      </div>
    </div>
  </ZnCard>
</template>

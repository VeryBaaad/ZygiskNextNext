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

import { MiuixCard, MiuixDropdownPreference, showSnackbar } from 'miuix-vue';
import { computed, ref } from 'vue';

import type { HookConfig, HookKind } from '../../api/config';
import { useAppActions } from '../../composables/app-actions';
import { HOOK_ROWS, UI_ROW_LABEL_KEY, hookOptionLabel, uiModeLabel } from '../../config-model';
import { t } from '../../i18n';
import { UI_MODES, isUiMode, setUiMode, uiMode } from '../../ui-mode';

interface ConfigRow {
  kind: string;
  label: string;
  value: string;
  ids: string[];
  labels: string[];
}

const props = defineProps<{ config: HookConfig }>();

const actions = useAppActions();

const busy = ref(false);

const rows = computed<ConfigRow[]>(() => {
  const out: ConfigRow[] = HOOK_ROWS.map((row) => {
    const entry = props.config[row.field];
    return {
      kind: row.kind,
      label: t(row.labelKey),
      value: entry.value,
      ids: [...entry.options],
      labels: entry.options.map((id) => hookOptionLabel(row.kind, id)),
    };
  });
  out.push({
    kind: 'ui',
    label: t(UI_ROW_LABEL_KEY),
    value: uiMode.value,
    ids: [...UI_MODES],
    labels: UI_MODES.map((id) => uiModeLabel(id)),
  });
  return out;
});

function selectedIndex(row: ConfigRow): number {
  const position = row.ids.indexOf(row.value);
  return position < 0 ? 0 : position;
}

async function select(row: ConfigRow, position: number): Promise<void> {
  const value = row.ids[position];
  if (!value || value === row.value) return;
  if (row.kind === 'ui') {
    if (isUiMode(value)) setUiMode(value);
    return;
  }
  if (busy.value) return;
  busy.value = true;
  try {
    await actions.applyHookConfig(row.kind as HookKind, value);
    void showSnackbar({ message: t('config.saved'), duration: 'short' });
  } catch (e) {
    console.warn('[znn] config-set:', e);
    void showSnackbar({ message: t('config.saveFailed'), duration: 'long' });
  } finally {
    busy.value = false;
  }
}
</script>

<template>
  <MiuixCard class="miuix-config-card">
    <MiuixDropdownPreference
      v-for="row in rows"
      :key="row.kind"
      class="miuix-config-row"
      :title="row.label"
      :items="row.labels"
      :model-value="selectedIndex(row)"
      @update:model-value="(position: number) => select(row, position)"
    />
  </MiuixCard>
</template>

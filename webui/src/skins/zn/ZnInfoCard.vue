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

import { MODULE_NAME, VER_NAME } from '../../app-info';
import type { InjectorStatus } from '../../api/injector';
import type { SystemInfo } from '../../api/system';
import { t } from '../../i18n';
import ZnCard from './ZnCard.vue';

const props = defineProps<{
  system: SystemInfo;
  status: InjectorStatus | null;
  error?: boolean;
}>();

interface InfoRow {
  label: string;
  value: string;
}

const abiText = computed(() => {
  const primary = (props.system.abi || '').trim();
  const rest = (props.system.abilist || '')
    .split(',')
    .map((value) => value.trim())
    .filter((value) => value.length > 0 && value !== primary);
  const parts = [primary ? `${primary} (primary)` : t('info.unknown')];
  parts.push(...rest);
  return parts.join(', ');
});

const modeText = computed(() => {
  const mode = props.status?.mode;
  if (mode === 'proc') return t('config.mode.proc');
  if (mode === 'ptrace') return t('config.mode.ptrace');
  return t('info.unknown');
});

const rows = computed<InfoRow[]>(() => [
  { label: MODULE_NAME, value: VER_NAME },
  { label: 'Kernel', value: props.system.kernel || t('info.unknown') },
  { label: 'Android SDK', value: props.system.sdk > 0 ? String(props.system.sdk) : t('info.unknown') },
  { label: 'ABI', value: abiText.value },
  { label: t('info.mode'), value: modeText.value },
]);
</script>

<template>
  <ZnCard :title="t('card.basic')" :error="props.error">
    <template v-if="!props.error">
      <template v-for="row in rows" :key="row.label">
        <div class="zn-divider" role="separator"></div>
        <div class="zn-field-label">{{ row.label }}</div>
        <div class="zn-field-value">{{ row.value }}</div>
      </template>
    </template>
  </ZnCard>
</template>

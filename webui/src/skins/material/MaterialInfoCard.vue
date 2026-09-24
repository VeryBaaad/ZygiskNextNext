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
import { cleanVersion, type SystemInfo } from '../../api/system';
import { t } from '../../i18n';

interface RootEntry {
  name: string;
  version: string | null;
}

interface InfoRow {
  label: string;
  value: string;
}

const props = defineProps<{
  system: SystemInfo;
  status: InjectorStatus | null;
}>();

const rootImplText = computed(() => {
  const entries: RootEntry[] = [
    { name: 'KernelSU', version: cleanVersion(props.system.root.kernelSU) },
    { name: 'Magisk', version: cleanVersion(props.system.root.magisk) },
    { name: 'APatch', version: cleanVersion(props.system.root.apatch) },
  ].filter((entry) => entry.version !== null);
  if (entries.length === 0) return t('info.rootImpl.none');
  return entries.map((entry) => `${entry.name} (${entry.version})`).join(', ');
});

const modeText = computed(() => {
  if (!props.status?.running) return t('info.unknown');
  if (props.status.mode === 'proc') return t('config.mode.proc');
  if (props.status.mode === 'ptrace') return t('config.mode.ptrace');
  return t('info.unknown');
});

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

const rows = computed<InfoRow[]>(() => {
  const sdk = props.system.sdk > 0 ? String(props.system.sdk) : t('info.unknown');
  return [
    { label: t('info.rootImpl'), value: rootImplText.value },
    { label: 'Kernel', value: props.system.kernel || t('info.unknown') },
    { label: 'Android SDK', value: sdk },
    { label: 'ABI', value: abiText.value },
    { label: t('info.mode'), value: modeText.value },
  ];
});
</script>

<template>
  <m3e-card class="info-card" variant="filled">
    <div slot="content" class="card-body">
      <div v-for="row in rows" :key="row.label" class="info-row">
        <span class="info-label">{{ row.label }}</span>
        <span class="info-value">{{ row.value }}</span>
      </div>
    </div>
  </m3e-card>
</template>

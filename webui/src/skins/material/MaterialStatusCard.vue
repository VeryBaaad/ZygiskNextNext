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

import { VER_NAME } from '../../app-info';
import type { InjectorStatus } from '../../api/injector';
import { t } from '../../i18n';

const props = defineProps<{ status: InjectorStatus }>();

const mode = computed(() => {
  if (!props.status.running) return '';
  if (props.status.mode === 'proc') return t('config.mode.proc');
  if (props.status.mode === 'ptrace') return t('config.mode.ptrace');
  return '';
});
</script>

<template>
  <m3e-card class="status-card" variant="filled">
    <div slot="content" class="status-body">
      <m3e-icon
        class="status-icon"
        :class="props.status.running ? 'is-active' : 'is-inactive'"
        :name="props.status.running ? 'check_circle' : 'error'"
        :filled="true"
      ></m3e-icon>
      <div class="status-text">
        <div class="status-label">{{ t(props.status.running ? 'status.active' : 'status.inactive') }}</div>
        <div class="status-version">{{ VER_NAME }}</div>
      </div>
      <span v-if="mode" class="status-badge">{{ mode }}</span>
    </div>
  </m3e-card>
</template>

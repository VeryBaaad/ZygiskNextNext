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

import { MiuixCard, MiuixIcon, MiuixText } from 'miuix-vue';
import { computed } from 'vue';

import { VER_NAME } from '../../app-info';
import type { InjectorStatus } from '../../api/injector';
import { t } from '../../i18n';
import { CHECK_CIRCLE, ERROR_CIRCLE, GLYPH_VIEW_BOX } from './glyphs';

const props = defineProps<{ status: InjectorStatus }>();

const active = computed(() => props.status.running);

const mode = computed(() => {
  if (!props.status.running) return '';
  if (props.status.mode === 'proc') return t('config.mode.proc');
  if (props.status.mode === 'ptrace') return t('config.mode.ptrace');
  return '';
});
</script>

<template>
  <MiuixCard
    class="miuix-status-card"
    :class="active ? 'is-active' : 'is-inactive'"
    press-feedback="none"
  >
    <div class="miuix-status-body">
      <span class="miuix-status-mark" aria-hidden="true">
        <MiuixIcon :size="110">
          <svg :viewBox="GLYPH_VIEW_BOX" fill="none" aria-hidden="true">
            <path
              :d="active ? CHECK_CIRCLE : ERROR_CIRCLE"
              fill="currentColor"
              fill-rule="nonzero"
              clip-rule="nonzero"
            />
          </svg>
        </MiuixIcon>
      </span>
      <div class="miuix-status-head">
        <MiuixText :size="22" weight="semibold">
          {{ t(active ? 'status.active' : 'status.inactive') }}
        </MiuixText>
        <MiuixText :size="15">{{ VER_NAME }}</MiuixText>
      </div>
      <div v-if="mode" class="miuix-status-mode">
        <MiuixText :size="16" weight="medium">{{ mode }}</MiuixText>
      </div>
    </div>
  </MiuixCard>
</template>

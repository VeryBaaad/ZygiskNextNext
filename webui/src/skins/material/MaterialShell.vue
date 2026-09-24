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

import './elements';

import { computed } from 'vue';

import type { HookConfig } from '../../api/config';
import type { InjectorStatus } from '../../api/injector';
import type { ZnnModule } from '../../api/modules';
import type { SystemInfo } from '../../api/system';
import MaterialConfigCard from './MaterialConfigCard.vue';
import MaterialEmptyCard from './MaterialEmptyCard.vue';
import MaterialFooter from './MaterialFooter.vue';
import MaterialInfoCard from './MaterialInfoCard.vue';
import MaterialModulesCard from './MaterialModulesCard.vue';
import MaterialStatusCard from './MaterialStatusCard.vue';
import MaterialTopBar from './MaterialTopBar.vue';

const props = defineProps<{
  ksuAvailable: boolean;
  loading: boolean;
  status: InjectorStatus | null;
  system: SystemInfo | null;
  modules: ZnnModule[] | null;
  config: HookConfig | null;
}>();

const empty = computed(
  () =>
    !props.ksuAvailable ||
    (!props.status && !props.system && !props.modules && !props.config),
);
</script>

<template>
  <MaterialTopBar :ksu-available="props.ksuAvailable" :refreshing="props.loading" />

  <main class="znn-content">
    <MaterialStatusCard v-if="props.status" :status="props.status" />
    <MaterialInfoCard v-if="props.system" :system="props.system" :status="props.status" />
    <MaterialConfigCard v-if="props.config" :config="props.config" />
    <MaterialModulesCard v-if="props.modules" :modules="props.modules" />
    <MaterialEmptyCard v-if="empty" />
  </main>

  <MaterialFooter />
</template>

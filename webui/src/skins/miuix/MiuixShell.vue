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

import 'miuix-vue/style.css';
import '../../styles/miuix.css';

import { MiuixScrollArea, MiuixSnackbarHost, setThemeMode as setMiuixThemeMode } from 'miuix-vue';
import { computed, watch } from 'vue';

import type { HookConfig } from '../../api/config';
import type { InjectorStatus } from '../../api/injector';
import type { ZnnModule } from '../../api/modules';
import type { SystemInfo } from '../../api/system';
import { themeScheme } from '../../theme';
import MiuixAppBar from './MiuixAppBar.vue';
import MiuixConfigCard from './MiuixConfigCard.vue';
import MiuixEmptyCard from './MiuixEmptyCard.vue';
import MiuixFooter from './MiuixFooter.vue';
import MiuixInfoCard from './MiuixInfoCard.vue';
import MiuixLargeTitle from './MiuixLargeTitle.vue';
import MiuixModulesCard from './MiuixModulesCard.vue';
import MiuixStatusCard from './MiuixStatusCard.vue';

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

/**
 * miuix-vue owns the `m-theme-dark` class and re-asserts its own default the
 * moment its module is evaluated, so the resolved scheme has to be pushed into
 * its controller as well as onto the document.
 */
watch(themeScheme, (scheme) => setMiuixThemeMode(scheme), { immediate: true });
</script>

<template>
  <div class="znn-miuix">
    <MiuixAppBar :ksu-available="props.ksuAvailable" :refreshing="props.loading" />

    <MiuixScrollArea class="znn-miuix-scroll">
      <MiuixLargeTitle />

      <main class="znn-content">
        <MiuixStatusCard v-if="props.status" :status="props.status" />
        <MiuixInfoCard v-if="props.system" :system="props.system" :status="props.status" />
        <MiuixConfigCard v-if="props.config" :config="props.config" />
        <MiuixModulesCard v-if="props.modules" :modules="props.modules" />
        <MiuixEmptyCard v-if="empty" />
      </main>

      <MiuixFooter />
    </MiuixScrollArea>

    <MiuixSnackbarHost />
  </div>
</template>

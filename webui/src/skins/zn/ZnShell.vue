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

import '../../styles/zn.css';

import { computed } from 'vue';

import type { HookConfig } from '../../api/config';
import type { InjectorStatus } from '../../api/injector';
import type { ZnnModule } from '../../api/modules';
import type { SystemInfo } from '../../api/system';
import { t } from '../../i18n';
import ZnAlert from './ZnAlert.vue';
import ZnConfigCard from './ZnConfigCard.vue';
import ZnEmptyCard from './ZnEmptyCard.vue';
import ZnFooter from './ZnFooter.vue';
import ZnInfoCard from './ZnInfoCard.vue';
import ZnStatusCard from './ZnStatusCard.vue';
import ZnTopBar from './ZnTopBar.vue';

const props = defineProps<{
  ksuAvailable: boolean;
  loading: boolean;
  status: InjectorStatus | null;
  system: SystemInfo | null;
  modules: ZnnModule[] | null;
  config: HookConfig | null;
}>();

function failed(present: boolean): boolean {
  return props.ksuAvailable && !props.loading && !present;
}

const modules = computed<ZnnModule[]>(() => props.modules ?? []);

const issueCount = computed(() =>
  modules.value.reduce((total, module) => total + (module.failed ?? []).length, 0),
);

const showDashboard = computed(() => props.ksuAvailable);

const EMPTY_SYSTEM: SystemInfo = {
  kernel: '',
  sdk: 0,
  abi: '',
  abilist: '',
  root: { magisk: null, kernelSU: null, apatch: null },
};

const EMPTY_CONFIG: HookConfig = {
  inlineHook: { value: '', options: [] },
  pltHook: { value: '', options: [] },
  mode: { value: '', options: [] },
};
</script>

<template>
  <div class="znn-zn">
    <ZnTopBar />

    <main class="zn-container">
      <ZnAlert v-if="showDashboard && issueCount > 0">
        {{ t('zn.issue.banner', { n: issueCount }) }}
      </ZnAlert>

      <template v-if="showDashboard">
        <ZnInfoCard
          v-if="props.system || failed(!!props.system)"
          :system="props.system ?? EMPTY_SYSTEM"
          :status="props.status"
          :error="failed(!!props.system)"
        />

        <ZnStatusCard
          :status="props.status"
          :root="props.system?.root ?? null"
          :modules="modules"
          :refreshing="props.loading"
          :error="failed(!!props.status)"
        />

        <ZnConfigCard
          v-if="props.config || failed(!!props.config)"
          :config="props.config ?? EMPTY_CONFIG"
          :error="failed(!!props.config)"
        />
      </template>

      <ZnEmptyCard v-else />
    </main>

    <ZnFooter />
  </div>
</template>

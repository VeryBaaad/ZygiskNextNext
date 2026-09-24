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

import { ref } from 'vue';

import ZnIcon from './ZnIcon.vue';

const props = withDefaults(defineProps<{ title: string; initiallyOpen?: boolean }>(), {
  initiallyOpen: false,
});

const expanded = ref(props.initiallyOpen);

function toggle(): void {
  expanded.value = !expanded.value;
}
</script>

<template>
  <div class="zn-collapse">
    <button
      class="zn-collapse-header"
      type="button"
      :aria-expanded="expanded"
      @click="toggle"
    >
      <span class="zn-collapse-title">{{ props.title }}</span>
      <ZnIcon class="zn-collapse-arrow" name="chevronDown" :size="18" />
    </button>
    <div v-if="expanded" class="zn-collapse-panel">
      <slot />
    </div>
  </div>
</template>

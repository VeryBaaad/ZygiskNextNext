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

import { computed } from 'vue';

import { resolveIcon, type IconName } from './icons';

const props = withDefaults(defineProps<{ name: IconName; size?: number }>(), { size: 16 });

const spec = computed(() => resolveIcon(props.name));
</script>

<template>
  <svg
    class="zn-icon"
    viewBox="0 0 24 24"
    :width="props.size"
    :height="props.size"
    fill="none"
    stroke="currentColor"
    :stroke-width="spec.strokeWidth"
    stroke-linecap="round"
    stroke-linejoin="round"
    aria-hidden="true"
    focusable="false"
  >
    <path v-for="d in spec.strokes" :key="d" :d="d" />
    <path v-for="d in spec.fills" :key="d" :d="d" fill="currentColor" stroke="none" />
  </svg>
</template>

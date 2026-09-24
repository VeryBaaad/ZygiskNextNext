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

import ZnIcon from './ZnIcon.vue';

const props = withDefaults(defineProps<{ title: string; error?: boolean }>(), { error: false });
</script>

<template>
  <section class="zn-card" :class="{ 'is-error': props.error }">
    <header class="zn-card-header">
      <div class="zn-card-header-main">{{ props.title }}</div>
      <div class="zn-card-header-extra">
        <ZnIcon v-if="props.error" name="warning" :size="20" />
        <slot name="extra" />
      </div>
    </header>
    <div class="zn-card-body">
      <slot />
    </div>
  </section>
</template>

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

import { computed, useTemplateRef } from 'vue';

import ZnIcon from './ZnIcon.vue';
import { usePopover } from './use-popover';

const props = defineProps<{
  modelValue: string;
  options: string[];
  label: (id: string) => string;
  disabled?: boolean;
}>();

const emit = defineEmits<{ select: [id: string] }>();

const root = useTemplateRef<HTMLElement>('root');

const { open, above, toggle, close } = usePopover(() => root.value);

const currentLabel = computed(() => props.label(props.modelValue));

function choose(id: string): void {
  close();
  if (id === props.modelValue) return;
  emit('select', id);
}
</script>

<template>
  <div ref="root" class="zn-select-wrap">
    <button
      class="zn-select"
      type="button"
      :disabled="props.disabled"
      :aria-expanded="open"
      aria-haspopup="listbox"
      @click="toggle()"
    >
      <span class="zn-select-value">{{ currentLabel }}</span>
      <ZnIcon class="zn-select-arrow" name="chevronDown" :size="16" />
    </button>

    <div v-if="open" class="zn-menu" :class="{ 'is-above': above }" role="listbox">
      <button
        v-for="id in props.options"
        :key="id"
        class="zn-menu-item"
        :class="{ 'is-selected': id === props.modelValue }"
        type="button"
        role="option"
        :aria-selected="id === props.modelValue"
        @click="choose(id)"
      >
        <span>{{ props.label(id) }}</span>
        <ZnIcon class="zn-menu-check" name="check" :size="16" />
      </button>
    </div>
  </div>
</template>

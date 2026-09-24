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

import { MiuixText } from 'miuix-vue';
import { onBeforeUnmount, onMounted, ref } from 'vue';

import { MODULE_NAME } from '../../app-info';

/**
 * The large title lives in the page scroll flow so it scrolls away under the
 * app bar, and drives two custom properties the skin CSS reads: the large title
 * fades out over the first third of its own height while the bar's small title
 * fades in at the same point. The published MiuixTopAppBar has no collapsing
 * form, so this is the replacement for it.
 */
const SHELL_SELECTOR = '.znn-miuix';

const LARGE_FADE_VAR = '--znn-miuix-large-fade';

const SMALL_ALPHA_VAR = '--znn-miuix-small-alpha';

const largeTitle = ref<HTMLElement | null>(null);

let scroller: HTMLElement | null = null;
let shell: HTMLElement | null = null;
let expansion = 0;
let frame = 0;

function findScroller(from: HTMLElement | null): HTMLElement | null {
  let node = from?.parentElement ?? null;
  while (node) {
    const overflowY = getComputedStyle(node).overflowY;
    if (overflowY === 'auto' || overflowY === 'scroll') return node;
    node = node.parentElement;
  }
  return null;
}

function measure(): void {
  const el = largeTitle.value;
  expansion = el ? el.offsetHeight : 0;
  shell = el?.closest<HTMLElement>(SHELL_SELECTOR) ?? null;
}

function update(): void {
  if (!scroller || !shell) return;
  if (expansion <= 0) measure();
  if (expansion <= 0) return;
  const fraction = Math.min(1, Math.max(0, scroller.scrollTop / expansion));
  const fade = Math.min(1, fraction * 3);
  shell.style.setProperty(LARGE_FADE_VAR, fade.toFixed(4));
  shell.style.setProperty(SMALL_ALPHA_VAR, fade >= 1 ? '1' : '0');
}

function scheduleUpdate(): void {
  if (frame) return;
  frame = requestAnimationFrame(() => {
    frame = 0;
    update();
  });
}

function onResize(): void {
  measure();
  scheduleUpdate();
}

onMounted(() => {
  measure();
  scroller = findScroller(largeTitle.value);
  scroller?.addEventListener('scroll', scheduleUpdate, { passive: true });
  window.addEventListener('resize', onResize, { passive: true });
  scheduleUpdate();
});

onBeforeUnmount(() => {
  scroller?.removeEventListener('scroll', scheduleUpdate);
  window.removeEventListener('resize', onResize);
  if (frame) cancelAnimationFrame(frame);
  frame = 0;
});
</script>

<template>
  <div ref="largeTitle" class="miuix-large-title">
    <MiuixText type="title1">{{ MODULE_NAME }}</MiuixText>
  </div>
</template>

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

import { nextTick, onBeforeUnmount, ref, watch, type Ref } from 'vue';

const VIEWPORT_MARGIN = 8;

export interface Popover {
  open: Ref<boolean>;
  above: Ref<boolean>;
  toggle: () => void;
  close: () => void;
}

export function usePopover(getRoot: () => HTMLElement | null): Popover {
  const open = ref(false);
  const above = ref(false);

  function close(): void {
    open.value = false;
  }

  function toggle(): void {
    open.value = !open.value;
  }

  function onPointerDown(event: Event): void {
    const element = getRoot();
    if (!element) return;
    if (event.target instanceof Node && element.contains(event.target)) return;
    close();
  }

  function onKeyDown(event: KeyboardEvent): void {
    if (event.key === 'Escape') close();
  }

  function reposition(): void {
    const element = getRoot();
    const panel = element?.querySelector<HTMLElement>('.zn-menu');
    if (!element || !panel) return;
    const anchor = element.getBoundingClientRect();
    const height = panel.getBoundingClientRect().height;
    const roomBelow = window.innerHeight - anchor.bottom - VIEWPORT_MARGIN;
    const roomAbove = anchor.top - VIEWPORT_MARGIN;
    above.value = height > roomBelow && roomAbove > roomBelow;
  }

  watch(open, (isOpen) => {
    if (isOpen) {
      document.addEventListener('pointerdown', onPointerDown, true);
      document.addEventListener('keydown', onKeyDown);
      window.addEventListener('resize', reposition, { passive: true });
      window.addEventListener('scroll', reposition, { passive: true });
      void nextTick(reposition);
      return;
    }
    document.removeEventListener('pointerdown', onPointerDown, true);
    document.removeEventListener('keydown', onKeyDown);
    window.removeEventListener('resize', reposition);
    window.removeEventListener('scroll', reposition);
    above.value = false;
  });

  onBeforeUnmount(() => {
    document.removeEventListener('pointerdown', onPointerDown, true);
    document.removeEventListener('keydown', onKeyDown);
    window.removeEventListener('resize', reposition);
    window.removeEventListener('scroll', reposition);
  });

  return { open, above, toggle, close };
}

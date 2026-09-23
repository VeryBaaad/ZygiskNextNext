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

import '@m3e/web/button';
import '@m3e/web/card';
import '@m3e/web/icon';
import '@m3e/web/menu';
import '@m3e/web/snackbar';

import { M3eSnackbar } from '@m3e/web/snackbar';

import { setHookConfig, type HookConfig, type HookEngineEntry, type HookKind } from '../api/config';
import { onLocaleChange, t } from '../i18n';
import { escapeHtml } from '../util/html';

const ENGINE_LABELS: Record<string, string> = {
  dobby: 'Dobby',
  shadowhook: 'ShadowHook',
  rv64hook: 'rv64hook',
  lsplt: 'LSPlt',
  bytehook: 'ByteHook',
  xhook: 'xHook',
};

const ROW_LABEL_KEYS: Record<HookKind, string> = {
  inline: 'config.inlineHook',
  plt: 'config.pltHook',
  mode: 'config.mode',
};

interface ConfigRow {
  kind: HookKind;
  field: keyof HookConfig;
}

const ROWS: ConfigRow[] = [
  { kind: 'inline', field: 'inlineHook' },
  { kind: 'plt', field: 'pltHook' },
  { kind: 'mode', field: 'mode' },
];

export class ConfigCard extends HTMLElement {
  private unsub?: () => void;
  private busy = false;

  constructor(private readonly data: HookConfig) {
    super();
  }

  connectedCallback(): void {
    this.render();
    this.unsub = onLocaleChange(() => this.render());
  }

  disconnectedCallback(): void {
    this.unsub?.();
    this.unsub = undefined;
  }

  private optionLabel(kind: HookKind, id: string): string {
    if (kind === 'mode') return t(`config.mode.${id}`);
    return ENGINE_LABELS[id] ?? id;
  }

  private rowHtml(kind: HookKind, entry: HookEngineEntry): string {
    const menuId = `znn-config-menu-${kind}`;
    const items = entry.options
      .map(
        (value) =>
          `<m3e-menu-item-radio data-kind="${kind}" data-value="${escapeHtml(value)}" ${
            value === entry.value ? 'checked' : ''
          }>${escapeHtml(this.optionLabel(kind, value))}</m3e-menu-item-radio>`,
      )
      .join('');
    return `
      <div class="config-row">
        <span class="config-label">${escapeHtml(t(ROW_LABEL_KEYS[kind]))}</span>
        <m3e-button class="config-value" size="extra-small" variant="tonal" data-kind="${kind}">
          <span>${escapeHtml(this.optionLabel(kind, entry.value))}</span>
          <m3e-icon slot="trailing-icon" name="arrow_drop_down"></m3e-icon>
        </m3e-button>
        <m3e-menu id="${menuId}" position-x="before">${items}</m3e-menu>
      </div>`;
  }

  private render(): void {
    this.innerHTML = `
      <m3e-card class="config-card" variant="filled">
        <div slot="content" class="card-body">
          ${ROWS.map((r) => this.rowHtml(r.kind, this.data[r.field])).join('')}
          <p class="config-hint">${escapeHtml(t('config.reloadHint'))}</p>
        </div>
      </m3e-card>`;

    this.querySelectorAll<HTMLElement & { disabled?: boolean }>('m3e-button[data-kind]').forEach(
      (button) => {
        button.addEventListener('click', () => {
          const kind = button.getAttribute('data-kind');
          if (!kind) return;
          const menu = this.querySelector<HTMLElement & { toggle(t: HTMLElement): Promise<void> }>(
            `#znn-config-menu-${kind}`,
          );
          if (!menu) return;
          const rect = button.getBoundingClientRect();
          const viewport = document.documentElement.clientWidth;
          menu.style.setProperty(
            '--znn-menu-inline-end',
            `${Math.max(8, Math.round(viewport - rect.right))}px`,
          );
          void menu.toggle(button);
        });
      },
    );

    this.querySelectorAll<HTMLElement>('m3e-menu-item-radio[data-kind]').forEach((item) => {
      item.addEventListener('click', () => {
        const kind = item.getAttribute('data-kind') as HookKind | null;
        const value = item.getAttribute('data-value');
        if (!kind || !value || this.busy) return;
        const row = ROWS.find((r) => r.kind === kind);
        if (!row || value === this.data[row.field].value) return;
        void this.apply(kind, value);
      });
    });
  }

  private async apply(kind: HookKind, value: string): Promise<void> {
    this.busy = true;
    try {
      const next = await setHookConfig(kind, value);
      this.data.inlineHook = next.inlineHook;
      this.data.pltHook = next.pltHook;
      this.data.mode = next.mode;
      M3eSnackbar.open(t('config.saved'));
    } catch (e) {
      console.warn('[znn] config-set:', e);
      M3eSnackbar.open(t('config.saveFailed'));
    } finally {
      this.busy = false;
      this.render();
    }
  }
}

customElements.define('znn-config-card', ConfigCard);

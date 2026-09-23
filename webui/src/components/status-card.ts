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

import '@m3e/web/card';
import '@m3e/icons/rounded';

import { VER_NAME } from '../app-info';
import type { InjectorStatus } from '../api/injector';
import { onLocaleChange, t } from '../i18n';
import { escapeHtml } from '../util/html';

export class StatusCard extends HTMLElement {
  private unsub?: () => void;

  constructor(private readonly data: InjectorStatus) {
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

  private modeText(): string {
    if (!this.data.running) return '';
    if (this.data.mode === 'proc') return t('config.mode.proc');
    if (this.data.mode === 'ptrace') return t('config.mode.ptrace');
    return '';
  }

  private render(): void {
    const active = this.data.running;
    const mode = this.modeText();
    this.innerHTML = `
      <m3e-card class="status-card" variant="filled">
        <div slot="content" class="status-body">
          <m3e-icon class="status-icon ${active ? 'is-active' : 'is-inactive'}"
                    name="${active ? 'check_circle' : 'error'}"
                    filled></m3e-icon>
          <div class="status-text">
            <div class="status-label">${escapeHtml(t(active ? 'status.active' : 'status.inactive'))}</div>
            <div class="status-version">${escapeHtml(VER_NAME)}</div>
          </div>
          ${mode ? `<span class="status-badge">${escapeHtml(mode)}</span>` : ''}
        </div>
      </m3e-card>`;
  }
}

customElements.define('znn-status-card', StatusCard);

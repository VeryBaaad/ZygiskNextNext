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
import '@m3e/web/icon';

import { onLocaleChange, t } from '../i18n';
import { escapeHtml } from '../util/html';

export class EmptyState extends HTMLElement {
  private unsub?: () => void;

  connectedCallback(): void {
    this.render();
    this.unsub = onLocaleChange(() => this.render());
  }

  disconnectedCallback(): void {
    this.unsub?.();
    this.unsub = undefined;
  }

  private render(): void {
    this.innerHTML = `
      <m3e-card class="empty-card" variant="filled">
        <div slot="content" class="empty-body">
          <m3e-icon name="info"></m3e-icon>
          <span>${escapeHtml(t('empty.title'))}</span>
        </div>
      </m3e-card>`;
  }
}

customElements.define('znn-empty-state', EmptyState);

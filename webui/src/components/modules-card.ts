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
import '@m3e/web/list';

import type { ZnnModule } from '../api/modules';
import { onLocaleChange, t } from '../i18n';
import { escapeHtml } from '../util/html';

const basename = (value: string): string => {
  const cut = value.lastIndexOf('/');
  return cut >= 0 ? value.slice(cut + 1) : value;
};

export class ModulesCard extends HTMLElement {
  private unsub?: () => void;
  private readonly expanded = new Set<string>();

  constructor(private readonly modules: ZnnModule[]) {
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

  private renderProcesses(m: ZnnModule): string {
    const failed = m.failed ?? [];
    const rows: string[] = [];

    if (m.processes.length === 0) {
      rows.push('<div class="proc-empty">—</div>');
    } else {
      for (const p of m.processes) {
        const name = basename(p.name);
        rows.push(`
          <div class="proc-row">
            <span class="proc-name" title="${escapeHtml(name)}">${escapeHtml(name)}</span>
            <span class="proc-pid">pid=${p.pid}</span>
          </div>`);
      }
    }

    if (failed.length > 0) {
      rows.push(`<div class="proc-failed-title">${escapeHtml(t('modules.failedTitle'))}</div>`);
      for (const f of failed) {
        const name = basename(f.name);
        rows.push(`
          <div class="proc-failed-row">
            <span class="proc-name" title="${escapeHtml(name)}">${escapeHtml(name)}</span>
            <span class="proc-failed-reason" title="${escapeHtml(f.reason)}">
              ${escapeHtml(f.reason)}
            </span>
          </div>`);
      }
    }

    return `<div slot="items" class="proc-list">${rows.join('')}</div>`;
  }

  private renderModule(m: ZnnModule): string {
    const isOpen = this.expanded.has(m.id);
    const failedCount = (m.failed ?? []).length;
    const label = `${m.name} (${m.id})`;
    const counts = [escapeHtml(t('modules.processes', { n: m.processes.length }))];
    if (failedCount > 0) {
      counts.push(escapeHtml(t('modules.failed', { n: failedCount })));
    }

    return `
      <m3e-expandable-list-item data-id="${escapeHtml(m.id)}" ${isOpen ? 'open' : ''}>
        <span class="module-line">
          <span class="module-name" title="${escapeHtml(label)}">${escapeHtml(label)}</span>
          <span class="module-count ${failedCount > 0 ? 'has-failed' : ''}">${counts.join(' · ')}</span>
        </span>
        ${this.renderProcesses(m)}
      </m3e-expandable-list-item>`;
  }

  private render(): void {
    const body =
      this.modules.length === 0
        ? `<div class="modules-empty">${escapeHtml(t('modules.noModules'))}</div>`
        : this.modules.map((m) => this.renderModule(m)).join('');

    this.innerHTML = `
      <m3e-card class="modules-card" variant="filled">
        <div slot="content" class="card-body">
          <m3e-list class="modules-list">${body}</m3e-list>
        </div>
      </m3e-card>`;

    this.querySelectorAll<HTMLElement>('m3e-expandable-list-item[data-id]').forEach((item) => {
      const id = item.getAttribute('data-id');
      if (!id) return;
      item.addEventListener('opened', () => this.expanded.add(id));
      item.addEventListener('closed', () => this.expanded.delete(id));
    });
  }
}

customElements.define('znn-modules-card', ModulesCard);

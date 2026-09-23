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

import type { InjectorStatus } from '../api/injector';
import { cleanVersion, type SystemInfo } from '../api/system';
import { onLocaleChange, t } from '../i18n';
import { escapeHtml } from '../util/html';

interface RootEntry {
  name: string;
  version: string | null;
}

interface InfoRow {
  label: string;
  value: string;
}

export class InfoCard extends HTMLElement {
  private unsub?: () => void;

  constructor(
    private readonly data: SystemInfo,
    private readonly status: InjectorStatus | null,
  ) {
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

  private rootImplText(): string {
    const entries: RootEntry[] = [
      { name: 'KernelSU', version: cleanVersion(this.data.root.kernelSU) },
      { name: 'Magisk', version: cleanVersion(this.data.root.magisk) },
      { name: 'APatch', version: cleanVersion(this.data.root.apatch) },
    ].filter((e) => e.version !== null);
    if (entries.length === 0) return t('info.rootImpl.none');
    return entries.map((e) => `${e.name} (${e.version})`).join(', ');
  }

  private modeText(): string {
    if (!this.status?.running) return t('info.unknown');
    if (this.status.mode === 'proc') return t('config.mode.proc');
    if (this.status.mode === 'ptrace') return t('config.mode.ptrace');
    return t('info.unknown');
  }

  private abiText(): string {
    const primary = (this.data.abi || '').trim();
    const rest = (this.data.abilist || '')
      .split(',')
      .map((s) => s.trim())
      .filter((s) => s.length > 0 && s !== primary);
    const parts = [primary ? `${primary} (primary)` : t('info.unknown')];
    parts.push(...rest);
    return parts.join(', ');
  }

  private rows(): InfoRow[] {
    const sdk = this.data.sdk > 0 ? String(this.data.sdk) : t('info.unknown');
    return [
      { label: t('info.rootImpl'), value: this.rootImplText() },
      { label: 'Kernel', value: this.data.kernel || t('info.unknown') },
      { label: 'Android SDK', value: sdk },
      { label: 'ABI', value: this.abiText() },
      { label: t('info.mode'), value: this.modeText() },
    ];
  }

  private render(): void {
    const rows = this.rows()
      .map(
        (row) => `
          <div class="info-row">
            <span class="info-label">${escapeHtml(row.label)}</span>
            <span class="info-value">${escapeHtml(row.value)}</span>
          </div>`,
      )
      .join('');

    this.innerHTML = `
      <m3e-card class="info-card" variant="filled">
        <div slot="content" class="card-body">
          ${rows}
        </div>
      </m3e-card>`;
  }
}

customElements.define('znn-info-card', InfoCard);

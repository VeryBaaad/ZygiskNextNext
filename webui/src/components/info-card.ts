import '@material/web/divider/divider.js';
import '@material/web/elevation/elevation.js';

import type { InjectorStatus } from '../api/injector';
import { cleanVersion, type SystemInfo } from '../api/system';
import { onLocaleChange, t } from '../i18n';
import { escapeHtml } from '../util/html';

interface RootEntry {
  name: string;
  version: string | null;
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

  private compatText(): string {
    if (!this.status?.running) return t('info.unknown');
    return t(this.status.zygiskCompat ? 'info.zygiskCompat.on' : 'info.zygiskCompat.off');
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

  private render(): void {
    const sdk = this.data.sdk > 0 ? String(this.data.sdk) : t('info.unknown');
    this.innerHTML = `
      <div class="md-card info-card">
        <md-elevation></md-elevation>
        <div class="card-body">
          <div class="card-title">${t('info.title')}</div>
          <md-divider></md-divider>
          <div class="info-row">
            <span class="info-label">${t('info.rootImpl')}</span>
            <span class="info-value">${escapeHtml(this.rootImplText())}</span>
          </div>
          <md-divider></md-divider>
          <div class="info-row">
            <span class="info-label">Kernel</span>
            <span class="info-value">${escapeHtml(this.data.kernel || t('info.unknown'))}</span>
          </div>
          <md-divider></md-divider>
          <div class="info-row">
            <span class="info-label">Android SDK</span>
            <span class="info-value">${escapeHtml(sdk)}</span>
          </div>
          <md-divider></md-divider>
          <div class="info-row">
            <span class="info-label">ABI</span>
            <span class="info-value">${escapeHtml(this.abiText())}</span>
          </div>
          <md-divider></md-divider>
          <div class="info-row">
            <span class="info-label">${t('info.zygiskCompat')}</span>
            <span class="info-value">${escapeHtml(this.compatText())}</span>
          </div>
        </div>
      </div>`;
  }
}

customElements.define('znn-info-card', InfoCard);

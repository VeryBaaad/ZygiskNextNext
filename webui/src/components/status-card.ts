import '@m3e/web/card';
import '@m3e/web/icon';

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
                    name="${active ? 'check_circle' : 'warning'}"></m3e-icon>
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

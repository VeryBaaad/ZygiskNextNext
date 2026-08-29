import '@material/web/elevation/elevation.js';
import '@material/web/icon/icon.js';

import { VER_NAME } from '../app-info';
import type { InjectorStatus } from '../api/injector';
import { onLocaleChange, t } from '../i18n';

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

  private render(): void {
    const active = this.data.running;
    this.innerHTML = `
      <div class="md-card status-card">
        <md-elevation></md-elevation>
        <div class="card-body status-row">
          <md-icon class="status-icon ${active ? 'is-active' : 'is-failed'}" style="font-size: 24px;">
            ${active ? 'check_circle' : 'warning'}
          </md-icon>
          <div class="status-text">
            <div class="status-label">${t(active ? 'status.active' : 'status.inactive')}</div>
            <div class="status-version">${VER_NAME}</div>
          </div>
        </div>
      </div>`;
  }
}

customElements.define('znn-status-card', StatusCard);

import '@material/web/elevation/elevation.js';
import '@material/web/icon/icon.js';

import { onLocaleChange, t } from '../i18n';

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
      <div class="md-card empty-card">
        <md-elevation></md-elevation>
        <div class="card-body empty-body">
          <md-icon>info</md-icon>
          <span>${t('empty.title')}</span>
        </div>
      </div>`;
  }
}

customElements.define('znn-empty-state', EmptyState);

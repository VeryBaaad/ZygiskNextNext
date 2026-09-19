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

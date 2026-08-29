import '@material/web/elevation/elevation.js';
import '@material/web/divider/divider.js';
import '@material/web/icon/icon.js';
import '@material/web/iconbutton/icon-button.js';

import type { ZnnModule } from '../api/modules';
import { onLocaleChange, t } from '../i18n';
import { escapeHtml } from '../util/html';

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

  private renderModule(m: ZnnModule): string {
    const isOpen = this.expanded.has(m.id);
    const procs = m.processes;
    const failed = m.failed ?? [];
    const label = `${m.name} (${m.id})`;
    return `
      <div class="module-row">
        <span class="module-name" title="${escapeHtml(label)}">${escapeHtml(label)}</span>
        <div class="module-pill" data-id="${escapeHtml(m.id)}" role="button" tabindex="0"
             aria-expanded="${isOpen ? 'true' : 'false'}">
          <span class="module-count">${t('modules.processes', { n: procs.length })}</span>
          ${failed.length > 0
            ? `<span class="module-failed-count">${t('modules.failed', { n: failed.length })}</span>`
            : ''}
          <md-icon-button class="module-expand"
                          aria-label="${isOpen ? t('modules.collapse') : t('modules.expand')}">
            <md-icon>${isOpen ? 'expand_less' : 'expand_more'}</md-icon>
          </md-icon-button>
        </div>
      </div>
      <div class="module-procs ${isOpen ? 'open' : ''}">
        ${procs.length === 0
          ? '<div class="proc-empty">—</div>'
          : procs
              .map(
                (p) => `
          <div class="proc-row">
            <span class="proc-name" title="${escapeHtml(p.name)}">${escapeHtml(p.name)}</span>
            <span class="proc-pid">pid=${p.pid}</span>
          </div>`,
              )
              .join('')}
        ${failed.length === 0
          ? ''
          : `<div class="proc-failed">
               <div class="proc-failed-title">${t('modules.failedTitle')}</div>
               ${failed
                 .map(
                   (f) => `
               <div class="proc-row proc-failed-row">
                 <span class="proc-name" title="${escapeHtml(f.name)}">${escapeHtml(f.name)}</span>
                 <span class="proc-failed-reason" title="${escapeHtml(f.reason)}">${escapeHtml(f.reason)}</span>
               </div>`,
                 )
                 .join('')}
             </div>`}
      </div>`;
  }

  private render(): void {
    this.innerHTML = `
      <div class="md-card modules-card">
        <md-elevation></md-elevation>
        <div class="card-body">
          <div class="card-title">${t('modules.title')}</div>
          <md-divider></md-divider>
          ${this.modules.length === 0
            ? `<div class="modules-empty">${t('modules.noModules')}</div>`
            : this.modules.map((m) => this.renderModule(m)).join('')}
        </div>
      </div>`;

    this.querySelectorAll<HTMLElement>('.module-pill').forEach((pill) => {
      pill.addEventListener('click', () => {
        const id = pill.dataset.id;
        if (!id) return;
        if (this.expanded.has(id)) {
          this.expanded.delete(id);
        } else {
          this.expanded.add(id);
        }
        this.render();
      });
    });
  }
}

customElements.define('znn-modules-card', ModulesCard);

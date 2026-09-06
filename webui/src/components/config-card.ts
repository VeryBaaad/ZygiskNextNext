import '@material/web/divider/divider.js';
import '@material/web/elevation/elevation.js';
import '@material/web/icon/icon.js';
import '@material/web/menu/menu.js';
import '@material/web/menu/menu-item.js';

import { toast } from 'kernelsu';

import { setHookConfig, type HookConfig, type HookEngineEntry, type HookKind } from '../api/config';
import { onLocaleChange, t } from '../i18n';
import { escapeHtml } from '../util/html';

const ENGINE_LABELS: Record<string, string> = {
  dobby: 'Dobby',
  shadowhook: 'ShadowHook',
  rv64hook: 'rv64hook',
  lsplt: 'LSPlt',
  bytehook: 'ByteHook',
};

export class ConfigCard extends HTMLElement {
  private unsub?: () => void;
  private busy = false;

  constructor(private readonly data: HookConfig) {
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

  private label(id: string): string {
    return ENGINE_LABELS[id] ?? id;
  }

  private rowHtml(kind: HookKind, entry: HookEngineEntry): string {
    const labelKey = kind === 'inline' ? 'config.inlineHook' : 'config.pltHook';
    const menuId = `config-menu-${kind}`;
    const btnId = `config-btn-${kind}`;
    const items = entry.options
      .map(
        (id) => `
          <md-menu-item data-kind="${kind}" data-engine="${escapeHtml(id)}">
            ${id === entry.value ? '<md-icon slot="start">check</md-icon>' : ''}
            <div slot="headline">${escapeHtml(this.label(id))}</div>
          </md-menu-item>`,
      )
      .join('');
    return `
      <div class="config-row">
        <span class="config-label">${t(labelKey)}</span>
        <span class="config-control">
          <button type="button" class="config-select" id="${btnId}">
            <span class="config-select-label">${escapeHtml(this.label(entry.value))}</span>
            <md-icon class="config-select-arrow">arrow_drop_down</md-icon>
          </button>
          <md-menu id="${menuId}" anchor="${btnId}" positioning="fixed">
            ${items}
          </md-menu>
        </span>
      </div>`;
  }

  private render(): void {
    const inline = this.data.inlineHook;
    const plt = this.data.pltHook;
    this.innerHTML = `
      <div class="md-card config-card">
        <md-elevation></md-elevation>
        <div class="card-body">
          <div class="card-title">${t('config.title')}</div>
          <md-divider></md-divider>
          ${this.rowHtml('inline', inline)}
          <md-divider></md-divider>
          ${this.rowHtml('plt', plt)}
          <md-divider></md-divider>
          <div class="config-hint">${t('config.reloadHint')}</div>
        </div>
      </div>`;

    this.querySelectorAll<HTMLElement>('.config-select').forEach((btn) => {
      btn.addEventListener('click', () => {
        const kind = btn.id.endsWith('-inline') ? 'inline' : 'plt';
        const menu = this.querySelector<{ open: boolean } & HTMLElement>(`#config-menu-${kind}`);
        if (menu) menu.open = !menu.open;
      });
    });

    this.querySelectorAll<HTMLElement>('md-menu-item[data-engine]').forEach((item) => {
      item.addEventListener('click', () => {
        const kind = item.getAttribute('data-kind') as HookKind | null;
        const engine = item.getAttribute('data-engine');
        if (!kind || !engine || this.busy) return;
        const current = kind === 'inline' ? this.data.inlineHook.value : this.data.pltHook.value;
        if (engine === current) return;
        this.apply(kind, engine);
      });
    });
  }

  private async apply(kind: HookKind, engine: string): Promise<void> {
    this.busy = true;
    try {
      const next = await setHookConfig(kind, engine);
      this.data.inlineHook = next.inlineHook;
      this.data.pltHook = next.pltHook;
      this.render();
      toast(t('config.saved'));
    } catch (e) {
      console.warn('[znn] config-set:', e);
      toast(t('config.saveFailed'));
    } finally {
      this.busy = false;
    }
  }
}

customElements.define('znn-config-card', ConfigCard);

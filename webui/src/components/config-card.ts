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

const ROW_LABEL_KEYS: Record<HookKind, string> = {
  inline: 'config.inlineHook',
  plt: 'config.pltHook',
  mode: 'config.mode',
};

interface ConfigRow {
  kind: HookKind;
  field: keyof HookConfig;
}

const ROWS: ConfigRow[] = [
  { kind: 'inline', field: 'inlineHook' },
  { kind: 'plt', field: 'pltHook' },
  { kind: 'mode', field: 'mode' },
];

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

  private optionLabel(kind: HookKind, id: string): string {
    if (kind === 'mode') return t(`config.mode.${id}`);
    return ENGINE_LABELS[id] ?? id;
  }

  private rowHtml(kind: HookKind, entry: HookEngineEntry): string {
    const menuId = `config-menu-${kind}`;
    const btnId = `config-btn-${kind}`;
    const items = entry.options
      .map(
        (id) => `
          <md-menu-item data-kind="${kind}" data-value="${escapeHtml(id)}">
            ${id === entry.value ? '<md-icon slot="start">check</md-icon>' : ''}
            <div slot="headline">${escapeHtml(this.optionLabel(kind, id))}</div>
          </md-menu-item>`,
      )
      .join('');
    return `
      <div class="config-row">
        <span class="config-label">${t(ROW_LABEL_KEYS[kind])}</span>
        <span class="config-control">
          <button type="button" class="config-select" id="${btnId}" data-kind="${kind}">
            <span class="config-select-label">${escapeHtml(this.optionLabel(kind, entry.value))}</span>
            <md-icon class="config-select-arrow">arrow_drop_down</md-icon>
          </button>
          <md-menu id="${menuId}" anchor="${btnId}" positioning="fixed">
            ${items}
          </md-menu>
        </span>
      </div>`;
  }

  private render(): void {
    this.innerHTML = `
      <div class="md-card config-card">
        <md-elevation></md-elevation>
        <div class="card-body">
          <div class="card-title">${t('config.title')}</div>
          ${ROWS.map((r) => `<md-divider></md-divider>${this.rowHtml(r.kind, this.data[r.field])}`).join('')}
          <md-divider></md-divider>
          <div class="config-hint">${t('config.reloadHint')}</div>
        </div>
      </div>`;

    this.querySelectorAll<HTMLElement>('.config-select').forEach((btn) => {
      btn.addEventListener('click', () => {
        const kind = btn.getAttribute('data-kind') as HookKind | null;
        if (!kind) return;
        const menu = this.querySelector<{ open: boolean } & HTMLElement>(`#config-menu-${kind}`);
        if (menu) menu.open = !menu.open;
      });
    });

    this.querySelectorAll<HTMLElement>('md-menu-item[data-kind]').forEach((item) => {
      item.addEventListener('click', () => {
        const kind = item.getAttribute('data-kind') as HookKind | null;
        const value = item.getAttribute('data-value');
        if (!kind || !value || this.busy) return;
        const row = ROWS.find((r) => r.kind === kind);
        if (!row || value === this.data[row.field].value) return;
        this.apply(kind, value);
      });
    });
  }

  private async apply(kind: HookKind, value: string): Promise<void> {
    this.busy = true;
    try {
      const next = await setHookConfig(kind, value);
      this.data.inlineHook = next.inlineHook;
      this.data.pltHook = next.pltHook;
      this.data.mode = next.mode;
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

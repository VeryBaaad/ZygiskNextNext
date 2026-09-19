import '@m3e/web/app-bar';
import '@m3e/web/icon';
import '@m3e/web/icon-button';
import '@m3e/web/menu';

import { MODULE_ID, MODULE_NAME } from '../app-info';
import { isKsuAvailable } from '../api/ksu';
import { getLocale, LOCALE_LABELS, onLocaleChange, setLocale, t, type Locale } from '../i18n';
import { getThemeMode, onThemeChange, setThemeMode, type ThemeMode } from '../theme';
import { escapeHtml } from '../util/html';

const THEME_ICONS: Record<ThemeMode, string> = {
  auto: 'brightness_auto',
  light: 'light_mode',
  dark: 'dark_mode',
};

const THEME_MODES: ThemeMode[] = ['auto', 'light', 'dark'];

const THEME_LABEL_KEYS: Record<ThemeMode, string> = {
  auto: 'theme.auto',
  light: 'theme.light',
  dark: 'theme.dark',
};

export class TopBar extends HTMLElement {
  private unsubLocale?: () => void;
  private unsubTheme?: () => void;
  private refreshing = false;

  set refresh(value: boolean) {
    if (value === this.refreshing) return;
    this.refreshing = value;
    this.render();
  }

  get refresh(): boolean {
    return this.refreshing;
  }

  connectedCallback(): void {
    this.render();
    this.unsubLocale = onLocaleChange(() => this.render());
    this.unsubTheme = onThemeChange(() => this.render());
  }

  disconnectedCallback(): void {
    this.unsubLocale?.();
    this.unsubTheme?.();
    this.unsubLocale = undefined;
    this.unsubTheme = undefined;
  }

  private render(): void {
    const locale = getLocale();
    const mode = getThemeMode();
    const refresh = isKsuAvailable()
      ? `
        <m3e-icon-button class="top-bar-refresh ${this.refreshing ? 'is-loading' : ''}"
                         slot="trailing" aria-label="${escapeHtml(t('actions.refresh'))}"
                         ${this.refreshing ? 'disabled' : ''}>
          <m3e-icon name="refresh"></m3e-icon>
        </m3e-icon-button>`
      : '';

    this.innerHTML = `
      <m3e-app-bar class="top-bar" size="small">
        <span slot="title" class="top-bar-title" title="${escapeHtml(MODULE_ID)}">${escapeHtml(MODULE_NAME)}</span>
        ${refresh}
        <m3e-icon-button slot="trailing" aria-label="${escapeHtml(t('topbar.themeLabel'))}">
          <m3e-menu-trigger for="znn-theme-menu">
            <m3e-icon name="${THEME_ICONS[mode]}"></m3e-icon>
          </m3e-menu-trigger>
        </m3e-icon-button>
        <m3e-icon-button slot="trailing" aria-label="${escapeHtml(t('topbar.langLabel'))}">
          <m3e-menu-trigger for="znn-locale-menu">
            <m3e-icon name="language"></m3e-icon>
          </m3e-menu-trigger>
        </m3e-icon-button>
      </m3e-app-bar>

      <m3e-menu id="znn-theme-menu" position-x="before">
        ${THEME_MODES.map(
          (m) => `
        <m3e-menu-item-radio data-mode="${m}" ${m === mode ? 'checked' : ''}>
          ${escapeHtml(t(THEME_LABEL_KEYS[m]))}
        </m3e-menu-item-radio>`,
        ).join('')}
      </m3e-menu>

      <m3e-menu id="znn-locale-menu" position-x="before">
        ${(Object.keys(LOCALE_LABELS) as Locale[])
          .map(
            (l) => `
        <m3e-menu-item-radio data-locale="${l}" ${l === locale ? 'checked' : ''}>
          ${escapeHtml(LOCALE_LABELS[l])}
        </m3e-menu-item-radio>`,
          )
          .join('')}
      </m3e-menu>
    `;

    this.querySelector('.top-bar-refresh')?.addEventListener('click', () => {
      this.dispatchEvent(new CustomEvent('znn-refresh'));
    });

    this.querySelectorAll<HTMLElement>('m3e-menu-item-radio[data-mode]').forEach((item) => {
      item.addEventListener('click', () => {
        const next = item.getAttribute('data-mode') as ThemeMode | null;
        if (next) setThemeMode(next);
      });
    });

    this.querySelectorAll<HTMLElement>('m3e-menu-item-radio[data-locale]').forEach((item) => {
      item.addEventListener('click', () => {
        const next = item.getAttribute('data-locale') as Locale | null;
        if (next) setLocale(next);
      });
    });
  }
}

customElements.define('znn-top-bar', TopBar);

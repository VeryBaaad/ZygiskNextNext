import '@material/web/icon/icon.js';
import '@material/web/iconbutton/icon-button.js';
import '@material/web/menu/menu.js';
import '@material/web/menu/menu-item.js';

import { MODULE_ID } from '../app-info';
import { getLocale, LOCALE_LABELS, onLocaleChange, setLocale, t, type Locale } from '../i18n';
import { cycleTheme, getThemeMode, type ThemeMode } from '../theme';

const THEME_ICONS: Record<ThemeMode, string> = {
  auto: 'brightness_auto',
  light: 'light_mode',
  dark: 'dark_mode',
};

export class TopBar extends HTMLElement {
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
    const locale = getLocale();
    this.innerHTML = `
      <header class="top-bar">
        <span class="top-bar-title" title="${MODULE_ID}">${MODULE_ID}</span>
        <div class="top-bar-actions">
          <md-icon-button id="theme-btn" aria-label="${t('topbar.themeLabel')}">
            <md-icon>${THEME_ICONS[getThemeMode()]}</md-icon>
          </md-icon-button>
          <md-icon-button id="lang-btn" aria-label="${t('topbar.langLabel')}">
            <md-icon>language</md-icon>
          </md-icon-button>
        </div>
      </header>
      <md-menu id="lang-menu" anchor="lang-btn" positioning="fixed">
        ${(Object.keys(LOCALE_LABELS) as Locale[])
          .map(
            (l) => `
          <md-menu-item data-locale="${l}">
            ${l === locale ? '<md-icon slot="start">check</md-icon>' : ''}
            <div slot="headline">${LOCALE_LABELS[l]}</div>
          </md-menu-item>`,
          )
          .join('')}
      </md-menu>
    `;

    this.querySelector('#theme-btn')!.addEventListener('click', () => {
      cycleTheme();
      this.render();
    });

    const langBtn = this.querySelector('#lang-btn')!;
    const menu = this.querySelector('#lang-menu') as unknown as { open: boolean } & HTMLElement;
    langBtn.addEventListener('click', () => {
      menu.open = !menu.open;
    });

    this.querySelectorAll('md-menu-item').forEach((item) => {
      item.addEventListener('click', () => {
        const loc = item.getAttribute('data-locale') as Locale | null;
        if (loc && loc !== locale) setLocale(loc);
      });
    });
  }
}

customElements.define('znn-top-bar', TopBar);

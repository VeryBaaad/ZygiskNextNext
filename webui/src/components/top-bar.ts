/*
 * This file is part of Zygisk Next Next.
 *
 * Zygisk Next Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zygisk Next Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zygisk Next Next. If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2026 VeryBaaad <verybaaad@outlook.com>
 */

import '@m3e/web/icon';
import '@m3e/web/icon-button';
import '@m3e/web/menu';

import { MODULE_ID, MODULE_NAME } from '../app-info';
import { isKsuAvailable } from '../api/ksu';
import { getLocale, LOCALE_LABELS, onLocaleChange, setLocale, t, type Locale } from '../i18n';
import { getThemeMode, onThemeChange, setThemeMode, type ThemeMode } from '../theme';
import { cubicBezier } from '../util/easing';
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

const PROGRESS_VAR = '--znn-bar-progress';

const TITLE_ALPHA_VAR = '--znn-bar-title-alpha';

const EXPANDED_VAR = '--znn-bar-expanded';

const COLLAPSED_VAR = '--znn-bar-collapsed';

const COLLAPSED_TITLE_EASING = cubicBezier(0.8, 0, 0.8, 0.15);

export class TopBar extends HTMLElement {
  private unsubLocale?: () => void;
  private unsubTheme?: () => void;
  private frame = 0;
  private collapseSpan = 0;
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
    window.addEventListener('scroll', this.onScroll, { passive: true });
    window.addEventListener('resize', this.onResize, { passive: true });
    this.scheduleUpdate();
  }

  disconnectedCallback(): void {
    this.unsubLocale?.();
    this.unsubTheme?.();
    this.unsubLocale = undefined;
    this.unsubTheme = undefined;
    window.removeEventListener('scroll', this.onScroll);
    window.removeEventListener('resize', this.onResize);
    if (this.frame) cancelAnimationFrame(this.frame);
    this.frame = 0;
  }

  private onScroll = (): void => {
    this.scheduleUpdate();
  };

  private onResize = (): void => {
    this.measureCollapseSpan();
    this.scheduleUpdate();
  };

  private measureCollapseSpan(): void {
    const styles = getComputedStyle(this);
    const expanded = parseFloat(styles.getPropertyValue(EXPANDED_VAR));
    const collapsed = parseFloat(styles.getPropertyValue(COLLAPSED_VAR));
    this.collapseSpan = expanded > collapsed ? expanded - collapsed : 1;
  }

  private scheduleUpdate(): void {
    if (this.frame) return;
    this.frame = requestAnimationFrame(() => {
      this.frame = 0;
      this.updateScrollState();
    });
  }

  private updateScrollState(): void {
    if (this.collapseSpan <= 0) this.measureCollapseSpan();
    const scrolled = window.scrollY || document.documentElement.scrollTop || 0;
    const progress = Math.min(1, Math.max(0, scrolled / this.collapseSpan));
    this.style.setProperty(PROGRESS_VAR, progress.toFixed(4));
    this.style.setProperty(TITLE_ALPHA_VAR, COLLAPSED_TITLE_EASING(progress).toFixed(4));
  }

  private render(): void {
    const locale = getLocale();
    const mode = getThemeMode();
    const refresh = isKsuAvailable()
      ? `
        <m3e-icon-button class="top-bar-refresh ${this.refreshing ? 'is-loading' : ''}"
                         aria-label="${escapeHtml(t('actions.refresh'))}"
                         ${this.refreshing ? 'disabled' : ''}>
          <m3e-icon name="refresh"></m3e-icon>
        </m3e-icon-button>`
      : '';

    this.innerHTML = `
      <header class="top-bar">
        <div class="top-bar-row">
          <h1 class="top-bar-title" title="${escapeHtml(MODULE_ID)}">${escapeHtml(MODULE_NAME)}</h1>
          <div class="top-bar-actions">
            ${refresh}
            <m3e-icon-button aria-label="${escapeHtml(t('topbar.themeLabel'))}">
              <m3e-menu-trigger for="znn-theme-menu">
                <m3e-icon name="${THEME_ICONS[mode]}"></m3e-icon>
              </m3e-menu-trigger>
            </m3e-icon-button>
            <m3e-icon-button aria-label="${escapeHtml(t('topbar.langLabel'))}">
              <m3e-menu-trigger for="znn-locale-menu">
                <m3e-icon name="language"></m3e-icon>
              </m3e-menu-trigger>
            </m3e-icon-button>
          </div>
        </div>
        <div class="top-bar-expanded">
          <span class="top-bar-title top-bar-expanded-title" aria-hidden="true">${escapeHtml(MODULE_NAME)}</span>
        </div>
      </header>

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

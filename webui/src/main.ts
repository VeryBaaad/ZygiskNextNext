import '@material-symbols/font-400/outlined.css';
import './styles.css';

import '@m3e/web/theme';

import { enableEdgeToEdge, fullScreen } from 'kernelsu';

import { MODULE_NAME, VER_NAME } from './app-info';
import { getInjectorStatus, type InjectorStatus } from './api/injector';
import { isKsuAvailable } from './api/ksu';
import { getModules, type ZnnModule } from './api/modules';
import { getSystemInfo, type SystemInfo } from './api/system';
import { getHookConfig, type HookConfig } from './api/config';
import { ConfigCard } from './components/config-card.js';
import { EmptyState } from './components/empty-state.js';
import { Footer } from './components/footer.js';
import { InfoCard } from './components/info-card.js';
import { ModulesCard } from './components/modules-card.js';
import { StatusCard } from './components/status-card.js';
import { TopBar } from './components/top-bar.js';
import { applyLocale, onLocaleChange } from './i18n';
import { initTheme } from './theme';

void TopBar;
void Footer;
void ConfigCard;

document.title = `${MODULE_NAME} ${VER_NAME}`;
applyLocale();
initTheme();

if (isKsuAvailable()) {
  try {
    enableEdgeToEdge(true);
    fullScreen(true);
  } catch {
  }
}

interface CardData {
  status: InjectorStatus | null;
  system: SystemInfo | null;
  modules: ZnnModule[] | null;
  config: HookConfig | null;
}

let cards: CardData = { status: null, system: null, modules: null, config: null };
let loading = false;

function topBar(): (HTMLElement & { refresh?: boolean }) | null {
  return document.querySelector('znn-top-bar');
}

function syncRefresh(): void {
  const bar = topBar();
  if (bar) bar.refresh = loading;
}

async function load(): Promise<void> {
  if (!isKsuAvailable() || loading) return;
  loading = true;
  syncRefresh();
  try {
    const [status, system, modules, config] = await Promise.all([
      getInjectorStatus().catch((e) => {
        console.warn('[znn] status:', e);
        return null;
      }),
      getSystemInfo().catch((e) => {
        console.warn('[znn] system:', e);
        return null;
      }),
      getModules().catch((e) => {
        console.warn('[znn] modules:', e);
        return null;
      }),
      getHookConfig().catch((e) => {
        console.warn('[znn] config:', e);
        return null;
      }),
    ]);
    cards = { status, system, modules, config };
  } finally {
    loading = false;
    syncRefresh();
  }
  render();
}

function render(): void {
  const content = document.getElementById('content');
  if (!content) return;
  content.replaceChildren();

  if (!isKsuAvailable()) {
    content.append(new EmptyState());
    return;
  }

  if (cards.status) content.append(new StatusCard(cards.status));
  if (cards.system) content.append(new InfoCard(cards.system, cards.status));
  if (cards.config) content.append(new ConfigCard(cards.config));
  if (cards.modules) content.append(new ModulesCard(cards.modules));

  if (!cards.status && !cards.system && !cards.modules && !cards.config) {
    content.append(new EmptyState());
  }
}

topBar()?.addEventListener('znn-refresh', () => {
  void load();
});

onLocaleChange(() => {
  render();
});

if (isKsuAvailable()) {
  void load();
} else {
  render();
}

import '@material-symbols/font-400/outlined.css';
import './styles.css';

import '@material/web/button/outlined-button.js';
import '@material/web/divider/divider.js';
import '@material/web/elevation/elevation.js';
import '@material/web/icon/icon.js';
import '@material/web/iconbutton/icon-button.js';
import '@material/web/menu/menu.js';
import '@material/web/menu/menu-item.js';

import { enableEdgeToEdge } from 'kernelsu';

import { MODULE_NAME, VER_NAME } from './app-info';
import { getInjectorStatus, type InjectorStatus } from './api/injector';
import { isKsuAvailable } from './api/ksu';
import { getModules, type ZnnModule } from './api/modules';
import { getSystemInfo, type SystemInfo } from './api/system';
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

document.title = `${MODULE_NAME} ${VER_NAME}`;
applyLocale();
initTheme();

if (isKsuAvailable()) {
  try {
    enableEdgeToEdge(true);
  } catch {
  }
}

interface CardData {
  status: InjectorStatus | null;
  system: SystemInfo | null;
  modules: ZnnModule[] | null;
}

let cards: CardData = { status: null, system: null, modules: null };

async function load(): Promise<void> {
  if (!isKsuAvailable()) return;
  const [status, system, modules] = await Promise.all([
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
  ]);
  cards = { status, system, modules };
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
  if (cards.modules) content.append(new ModulesCard(cards.modules));

  if (!cards.status && !cards.system && !cards.modules) {
    content.append(new EmptyState());
  }
}

onLocaleChange(() => render());
void load();

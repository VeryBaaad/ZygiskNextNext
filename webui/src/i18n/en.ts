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

export const en: Record<string, string> = {
  'topbar.themeLabel': 'Theme',
  'topbar.langLabel': 'Language',

  'theme.auto': 'Follow system',
  'theme.light': 'Light',
  'theme.dark': 'Dark',

  'actions.refresh': 'Refresh',

  'status.active': 'Active',
  'status.inactive': 'Inactive',

  'info.rootImpl': 'Root Implementation',
  'info.rootImpl.none': 'None detected',
  'info.mode': 'Current Mode',
  'info.unknown': 'Unknown',

  'modules.processes': '{n} processes',
  'modules.failed': '{n} failed',
  'modules.failedTitle': 'Injection failures',
  'modules.noModules': 'No modules',

  'config.inlineHook': 'Inline Hook implementation',
  'config.pltHook': 'PLT Hook implementation',
  'config.mode': 'Tracking Mode',
  'config.mode.auto': 'Auto',
  'config.mode.ptrace': 'ptrace mode',
  'config.mode.proc': 'proc mode',
  'config.reloadHint':
    'Hook engines apply to newly started target processes; already-running processes must be restarted to pick them up. Tracking mode is applied when the injector starts: proc takes effect immediately, ptrace after a reboot.',
  'config.saved': 'Saved',
  'config.saveFailed': 'Failed to save configuration',

  'empty.title': 'No cards to display',

  'footer.github': 'Github',
};

export type Dictionary = Record<string, string>;

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

import type { HookConfig, HookKind } from './api/config';
import { t } from './i18n';

export const ENGINE_LABELS: Record<string, string> = {
  dobby: 'Dobby',
  shadowhook: 'ShadowHook',
  rv64hook: 'rv64hook',
  lsplt: 'LSPlt',
  bytehook: 'ByteHook',
  xhook: 'xHook',
};

export interface HookRow {
  kind: HookKind;
  field: keyof HookConfig;
  labelKey: string;
}

export const HOOK_ROWS: HookRow[] = [
  { kind: 'inline', field: 'inlineHook', labelKey: 'config.inlineHook' },
  { kind: 'plt', field: 'pltHook', labelKey: 'config.pltHook' },
  { kind: 'mode', field: 'mode', labelKey: 'config.mode' },
];

export const UI_ROW_LABEL_KEY = 'config.uiStyle';

export function hookOptionLabel(kind: HookKind, id: string): string {
  if (kind === 'mode') return t(`config.mode.${id}`);
  return ENGINE_LABELS[id] ?? id;
}

export function uiModeLabel(mode: string): string {
  if (mode === 'miuix') return t('ui.miuix');
  if (mode === 'zn') return t('ui.zn');
  return t('ui.material');
}

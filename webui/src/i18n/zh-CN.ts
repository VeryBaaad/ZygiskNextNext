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

import type { Dictionary } from './en';

export const zhCN: Dictionary = {
  'topbar.themeLabel': '主题',
  'topbar.langLabel': '语言',

  'theme.auto': '跟随系统',
  'theme.light': '浅色',
  'theme.dark': '深色',

  'actions.refresh': '刷新',

  'status.active': '已激活',
  'status.inactive': '未激活',

  'info.rootImpl': 'Root 实现',
  'info.rootImpl.none': '未检测到',
  'info.mode': '当前模式',
  'info.unknown': '未知',

  'modules.processes': '{n} 个进程',
  'modules.failed': '{n} 个失败',
  'modules.failedTitle': '注入失败',
  'modules.noModules': '无模块',
  'modules.noProcesses': '无进程',

  'config.inlineHook': 'Inline Hook 实现',
  'config.pltHook': 'PLT Hook 实现',
  'config.mode': '跟踪模式',
  'config.mode.auto': '自动',
  'config.mode.ptrace': 'ptrace mode',
  'config.mode.proc': 'proc mode',
  'config.uiStyle': '界面样式',
  'config.saved': '已保存',
  'config.saveFailed': '保存失败',

  'ui.material': 'Material',
  'ui.miuix': 'Miuix',

  'empty.title': '无卡片可绘制',

  'footer.github': 'Github',
};

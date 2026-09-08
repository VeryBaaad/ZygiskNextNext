import type { Dictionary } from './en';

export const zhCN: Dictionary = {
  'topbar.themeLabel': '切换主题',
  'topbar.langLabel': '语言',

  'status.active': '已激活',
  'status.inactive': '未激活',

  'info.title': '基本信息',
  'info.rootImpl': 'Root实现',
  'info.rootImpl.none': '未检测到',
  'info.mode': '当前模式',
  'info.unknown': '未知',

  'modules.title': '模块',
  'modules.processes': '{n} 个进程',
  'modules.failed': '{n} 个失败',
  'modules.failedTitle': '注入失败',
  'modules.noModules': '无模块',
  'modules.expand': '展开',
  'modules.collapse': '收起',

  'config.title': '配置',
  'config.inlineHook': 'Inline Hook 实现',
  'config.pltHook': 'PLT Hook 实现',
  'config.mode': '跟踪模式',
  'config.mode.auto': '自动',
  'config.mode.ptrace': 'ptrace mode',
  'config.mode.proc': 'proc mode',
  'config.reloadHint':
    'Hook 引擎对新启动的目标进程生效，已运行的进程需重启后才会使用新引擎。跟踪模式在注入器启动时生效：proc 立即生效，ptrace 需重启后生效。',
  'config.saved': '已保存',
  'config.saveFailed': '保存失败',

  'empty.title': '无卡片可绘制',

  'footer.github': 'Github',
};

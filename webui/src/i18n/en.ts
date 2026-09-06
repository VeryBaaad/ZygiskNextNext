export const en: Record<string, string> = {
  'topbar.themeLabel': 'Toggle theme',
  'topbar.langLabel': 'Language',

  'status.active': 'Active',
  'status.inactive': 'Inactive',

  'info.title': 'Basic Info',
  'info.rootImpl': 'Root Implementation',
  'info.rootImpl.none': 'None detected',
  'info.zygiskCompat': 'Zygisk Compat Mode',
  'info.zygiskCompat.on': 'Enabled',
  'info.zygiskCompat.off': 'Disabled',
  'info.unknown': 'Unknown',

  'modules.title': 'Modules',
  'modules.processes': '{n} processes',
  'modules.failed': '{n} failed',
  'modules.failedTitle': 'Injection failures',
  'modules.noModules': 'No modules',
  'modules.expand': 'Expand',
  'modules.collapse': 'Collapse',

  'config.title': 'Configuration',
  'config.inlineHook': 'Inline Hook implementation',
  'config.pltHook': 'PLT Hook implementation',
  'config.reloadHint':
    'Newly started processes use the selected engines. A restarting the target processes is required for the change to take effect.',
  'config.saved': 'Saved',
  'config.saveFailed': 'Failed to save configuration',

  'empty.title': 'No cards to display',

  'footer.github': 'Github',
};

export type Dictionary = Record<string, string>;

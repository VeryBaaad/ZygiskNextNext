export const en: Record<string, string> = {
  'topbar.themeLabel': 'Toggle theme',
  'topbar.langLabel': 'Language',

  'status.active': 'Active',
  'status.inactive': 'Inactive',

  'info.title': 'Basic Info',
  'info.rootImpl': 'Root Implementation',
  'info.rootImpl.none': 'None detected',
  'info.mode': 'Current Mode',
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

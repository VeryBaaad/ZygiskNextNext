(function () {
  if (window.ksu) return;

  var params = new URLSearchParams(location.search);

  var theme = params.get('theme');
  if (theme === 'light' || theme === 'dark' || theme === 'auto') {
    try {
      localStorage.setItem('znn_theme', theme);
    } catch (error) {}
  }

  var locale = params.get('locale');
  if (locale === 'en' || locale === 'zh-CN') {
    try {
      localStorage.setItem('znn_locale', locale);
    } catch (error) {}
  }

  var ui = params.get('ui');
  if (ui === 'material' || ui === 'miuix' || ui === 'zn') {
    try {
      localStorage.setItem('znn_ui', ui);
    } catch (error) {}
  }

  if (params.get('ksu') === '0') return;

  var latency = Number(params.get('slow') || 120);
  var failures = (params.get('fail') || '').split(',').filter(Boolean);
  var pid = function () {
    return 300 + Math.floor(Math.random() * 9000);
  };

  var CONFIG_KEYS = { inline: 'inlineHook', plt: 'pltHook', mode: 'mode' };

  var config = {
    inlineHook: { value: 'dobby', options: ['dobby', 'shadowhook', 'rv64hook'] },
    pltHook: { value: 'lsplt', options: ['lsplt', 'bytehook', 'xhook'] },
    mode: { value: 'proc', options: ['auto', 'proc', 'ptrace'] },
  };

  var modules = [
    {
      id: 'zygisknextsu',
      name: 'Zygisk Next Next',
      version: 'v0.0.0',
      processes: [
        { pid: pid(), name: 'adbd' },
        { pid: pid(), name: 'netd' },
        { pid: pid(), name: 'artd' },
      ],
      failed: [],
    },
    {
      id: 'zygisk_lsposed',
      name: 'LSPosed',
      version: 'v1.9.2',
      processes: [
        { pid: pid(), name: 'artd' },
        { pid: pid(), name: 'hyos_spawner' },
      ],
      failed: [
        { name: 'artd', reason: 'test' },
        { name: 'hyos_spawner', reason: 'test' },
      ],
    },
  ];

  if (params.get('modules') === 'none') {
    modules = [];
  }

  var state = {
    running: params.get('status') !== 'inactive',
    mode: 'proc',
    config: config,
    modules: modules,
  };

  function snapshot(cmd, operands) {
    switch (cmd) {
      case 'status':
        return { running: state.running, pid: state.running ? 4242 : 0, mode: state.mode };
      case 'system':
        return {
          kernel: '5.15.148-android13-8-g2b4c1f0a1b2c-ab1234567',
          sdk: 34,
          abi: 'arm64-v8a',
          abilist: 'arm64-v8a,armeabi-v7a,armeabi',
          root: { magisk: null, kernelSU: 'ksud v3.0.0', apatch: null },
        };
      case 'modules':
        return state.modules;
      case 'config':
        return state.config;
      case 'config-set': {
        var kind = operands[0];
        var value = operands[1];
        var key = CONFIG_KEYS[kind];
        var entry = key ? state.config[key] : null;
        if (!entry) {
          throw new Error('usage: injector --ctl config-set <inline|plt|mode> <value>');
        }
        if (entry.options.indexOf(value) < 0) {
          throw new Error('invalid ' + kind + ' value "' + value + '" for this device');
        }
        entry.value = value;
        return state.config;
      }
      default:
        throw new Error('unknown command: ' + cmd);
    }
  }

  window.ksu = {
    exec: function (command, options, callbackName) {
      window.setTimeout(function () {
        var argc = command.split(/\s+/);
        var ctl = argc.indexOf('--ctl');
        var cmd = ctl >= 0 ? argc[ctl + 1] : '';
        var operands = ctl >= 0 ? argc.slice(ctl + 2) : [];
        if (failures.indexOf(cmd) >= 0) {
          window[callbackName](1, '', 'stub: forced failure for "' + cmd + '"');
          return;
        }
        try {
          window[callbackName](0, JSON.stringify(snapshot(cmd, operands)), '');
        } catch (error) {
          window[callbackName](1, '', String(error && error.message ? error.message : error));
        }
      }, latency);
    },
    moduleInfo: function () {
      return JSON.stringify({ moduleDir: '/data/adb/modules/zygisknextsu' });
    },
    toast: function (message) {
      console.info('[znn preview] toast:', message);
    },
    fullScreen: function () {},
    enableEdgeToEdge: function () {},
    listPackages: function () {
      return '[]';
    },
  };

  console.info(
    '[znn preview] KernelSU bridge stubbed. Overrides: ?ksu=0 ?status=inactive ?modules=none ' +
      '?ui=material|miuix|zn ?theme=light|dark|auto ?locale=zh-CN|en ?slow=MS ?fail=config,modules',
  );
})();

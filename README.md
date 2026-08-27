# Zygisk Next Next

A from-scratch, standalone implementation of the **Zygisk Next module**.

## Introduction

A Magisk module that attempts to implement the ZygiskNext API, using Dobby and LSPlt for inline hooking or PLT hooking.

## Supported Versions

Android 8+

## Install

1. Install Magisk 26402+ or KernelSU 11425+ or APatch 10700+
2. [Download](#download) and install ZygiskNextNext in manager
3. Reboot
4. Have fun :)

## Download

- For stable releases, please go to [Github Releases page](https://github.com/VeryBaaad/ZygiskNextNext/releases)
- For canary build, please check [Github Actions](https://github.com/VeryBaaad/ZygiskNextNext/actions/workflows/ci.yml?query=branch%3Amaster)

Note: debug builds are only available in Github Actions.

## Get Help

**Only bug reports from **THE LATEST DEBUG BUILD** will be accepted.**

- GitHub issues: [Issues](https://github.com/VeryBaaad/ZygiskNextNext/issues/)

- (For Chinese speakers) 本项目只接受英语**标题**的issue。如果您不懂英语，请使用[翻译工具](https://www.deepl.com/zh/translator)

## For Developers

Read the [ZygiskNextModuleSample](https://github.com/5ec1cff/ZygiskNextModuleSample)

### HyperOS Runtime

Look this: [hyos_runtime.md](https://github.com/Dr-TSNG/ZygiskNext/blob/main/docs/hyos_runtime.md)

## Community Discussion

- Telegram: [@EdLSPesodITed](https://t.me/EdLSPesodITed)

Notice: These community groups don't accept any bug report, please use [Get help](#get-help) to report.

## Translation Contributing

There is no need for translation here for the time being.

## Credits 

- [Magisk](https://github.com/topjohnwu/Magisk/): makes all these possible
- [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext): OG implementation
- [Dobby](https://github.com/jmpews/Dobby): used for inline hooking
- [LSPlt](https://github.com/LSPosed/LSPlt): uesd for plt hooking
- [LZMA SDK](https://www.7-zip.org/sdk.html): ELF Parser

## License

Zygisk Next Next is licensed under the **GNU General Public License v3 (GPL-3)** (http://www.gnu.org/copyleft/gpl.html).

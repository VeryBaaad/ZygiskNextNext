# Zygisk Next Next

A from-scratch, standalone implementation of the **Zygisk Next module**.

## Introduction

A Magisk module that attempts to implement the ZygiskNext API, using Dobby and LSPlt for inline hooking or PLT hooking.

> A Zygisk Next module is a valid Magisk module. Starting with version v1.2.2—released after the project went closed-source—ZygiskNext introduced support for "ZygiskNext modules." These provide more convenient interfaces for inline and PLT hooking and even support advanced injection into the HyperOS Runtime. Consequently, I created this open-source Magisk module—named "ZygiskNextNext"—to load those ZygiskNext modules. Note: ZygiskNextNext is not a Zygisk implementation itself; If you need to use Zygisk, you must enable the Zygisk implementation itself, rather than using only ZygiskNextNext.

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

Create a Pull Request.

## Credits 

- [Magisk](https://github.com/topjohnwu/Magisk/): makes all these possible
- [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext): OG implementation
- [Dobby](https://github.com/jmpews/Dobby): used for inline hooking [arm64, arm, x64, x86]
- [rv64hook](https://github.com/eirv/riscv64-inline-hook): used for inline hooking [RISC-V 64]
- [LSPlt](https://github.com/LSPosed/LSPlt): uesd for plt hooking
- [LZMA SDK](https://www.7-zip.org/sdk.html): ELF Parser

## License

Zygisk Next Next is licensed under the **GNU General Public License v3 (GPL-3)** (http://www.gnu.org/copyleft/gpl.html).

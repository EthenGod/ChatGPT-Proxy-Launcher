[简体中文](release-upload.md) | [English](release-upload.en.md) | [日本語](release-upload.ja.md)

# v1.2.6 改进 ChatGPT 托盘退出

[![快速开始：查看分步教程](https://img.shields.io/badge/%E7%AC%AC%E4%B8%80%E6%AC%A1%E4%BD%BF%E7%94%A8-%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.6/README.md#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B) [![下载 Windows x64 配置器](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.6/ChatGPTLauncherSetup.exe)

**第一次使用请先看[快速开始](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.6/README.md#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B)。** 教程中的一行 PowerShell 命令默认下载并打开最新正式版配置器。手动下载和首次设置也有分步说明。

**从旧版升级**，双击打开新版配置器，点 **保存更改** 即可更新启动器。[直接下载 v1.2.6 配置器](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.6/ChatGPTLauncherSetup.exe)

如果开启了 **应用于 Codex 环境变量**，请先保存 ChatGPT 和 Codex 中的工作。保存设置后，配置器会尝试退出 ChatGPT。

## ✨ 更新内容

- 一键下载脚本打开配置器后，会提示你转到配置器窗口继续操作。

## 🛠️ 修复内容

- 保存设置需要退出 ChatGPT 时，优先使用托盘菜单的 Exit。菜单不可用时尝试 Ctrl+Q；等待超时会提示手动退出。

## ⚠️ 已知问题

- **静默启动** 暂不可用，启动时仍可能看到 ChatGPT 主窗口。
- 如果配置器提示 ChatGPT 仍在运行，请手动完全退出，再从 **ChatGPT Launcher** 快捷方式打开。

[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.ja.md)

# 🚀 v1.2.1 全新配置界面

[![快速开始：查看分步教程](https://img.shields.io/badge/%E7%AC%AC%E4%B8%80%E6%AC%A1%E4%BD%BF%E7%94%A8-%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/README.md#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B) [![下载 Windows x64 配置器](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe)

**第一次使用请先看[快速开始](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/README.md#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B)。** 教程中的一行 PowerShell 命令默认下载并打开最新正式版配置器。手动下载和首次设置也有分步说明。

**从旧版升级**，双击打开新版配置器，点 **保存更改** 即可更新启动器。[直接下载 v1.2.1 配置器](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe)

如果开启了 **应用于 Codex**，请先保存 ChatGPT 和 Codex 中的工作。保存设置后，配置器会尝试退出 ChatGPT。

## ✨ 更新内容

- 登录自启、桌面快捷方式和代理集中在首页，改动选项后就能预览启动参数。
- 打开代理开关，配置器会自动查找端口，找到就填好。想核对来源，可以点右下角的提示。
- 在 **高级设置** 里可以查看和添加随 ChatGPT 启动的程序。列表会标出哪些已适配、哪些已停用。
- 电脑缺少 WebView2 时，双击配置器会自动打开兼容界面，照样可以改设置。
- 通过 PowerShell 下载时默认获取正式版，想尝鲜也可以选择测试版。

## 🛠️ 修复内容

- 修复了切换语言时整个窗口闪黑一下的问题。切换后，刚填的代理地址和端口也会保留。
- 配置器最小化后，再次双击会回到原来的窗口，可以接着改刚才的设置。
- 修复了设置已经保存，界面却提示保存失败的问题。

## ⚠️ 已知问题

- **静默启动** 暂不可用，启动时仍可能看到 ChatGPT 主窗口。
- 如果配置器提示 ChatGPT 仍在运行，请手动完全退出，再重新打开，让新的 Codex 代理设置生效。

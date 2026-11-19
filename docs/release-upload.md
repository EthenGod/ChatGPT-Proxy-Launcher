[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.ja.md)

# 🚀 v1.2.1 全新配置界面

[![下载 Windows x64 配置器](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe)

双击下载的配置器，登录自启、桌面快捷方式和代理都能在首页设好。从旧版升级，点 **保存更改** 就能更新启动器。

[直接下载配置器](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe) ｜ [使用说明](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/README.md)

## ✨ 更新内容

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
- 启用 **应用于 Codex** 后，配置器在保存设置时会尝试退出 ChatGPT。请先保存手头的工作，必要时按提示手动退出再打开。

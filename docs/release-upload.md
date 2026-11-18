[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.0-beta.1/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.0-beta.1/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.0-beta.1/docs/release-upload.ja.md)

# v1.2.0 Beta 1

[![下载 Windows x64 配置器](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.0-beta.1/ChatGPTLauncherSetup.exe)

下载后双击打开。已有部署可以点击 **保存更改** 更新启动器。[直接下载配置器](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.0-beta.1/ChatGPTLauncherSetup.exe) ｜ [使用说明](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.0-beta.1/README.md)

## 更新内容

- 配置器换用新界面，支持中、英、日三种语言和深浅色。打开代理开关会检测一次端口，点击提示可查看来源。
- 高级设置会列出附加程序，并标出已适配的程序。
- 缺少 WebView2 时，PowerShell 会询问是否安装或使用原生兼容界面。直接双击配置器会自动打开兼容界面。
- PowerShell 默认只取正式版。传入 `-Channel Preview` 可包含测试版，`-Branch` 可选择分支，`-Tag` 可固定版本。

## 使用提醒

静默启动仍不可用。启用 **应用于 Codex** 时，请在保存前保存 ChatGPT 中的工作。配置器会尝试退出 ChatGPT，必要时会提示你手动退出并重新打开。

[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.ja.md)

# v1.2.1

[![下载 Windows x64 配置器](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe)

下载后双击打开。已有部署可以点击 **保存更改** 更新启动器。[直接下载配置器](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe) ｜ [使用说明](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/README.md)

## 更新内容

- 新版配置器把启动选项、代理和参数预览放在同一页。开启代理时会自动检测一次端口，点击右下角的提示可查看来源。
- 在 **高级设置** 中可以查看随 ChatGPT 启动的附加程序。已适配的程序有单独标记，点击名称可查看路径和启动参数。
- PowerShell 默认下载正式版。使用 `-Channel Preview` 可包含测试版，也可用 `-Branch` 筛选分支或用 `-Tag` 指定版本。

## 修复内容

- 配置器最小化后再次打开，会恢复原窗口。连续点击保存只会写入一次。
- 保存后会重新读取部署结果。如果写入成功但重新读取失败，界面会保留已保存状态并提示重新打开配置器。

## 使用提醒

缺少 WebView2 时，双击配置器会打开兼容界面。通过 PowerShell 打开时，也可以选择安装 WebView2。

静默启动仍不可用。启用 **应用于 Codex** 时，先保存 ChatGPT 中的工作，再保存设置。配置器会尝试退出 ChatGPT。

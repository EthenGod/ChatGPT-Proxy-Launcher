[简体中文](release-upload.md) | [English](release-upload.en.md) | [日本語](release-upload.ja.md)

# v1.2.3 配置器更小，启动器准备代理更快

[![快速开始：查看分步教程](https://img.shields.io/badge/%E7%AC%AC%E4%B8%80%E6%AC%A1%E4%BD%BF%E7%94%A8-%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.3/README.md#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B) [![下载 Windows x64 配置器](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.3/ChatGPTLauncherSetup.exe)

**第一次使用请先看[快速开始](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.3/README.md#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B)。** 教程中的一行 PowerShell 命令默认下载并打开最新正式版配置器。手动下载和首次设置也有分步说明。

**从旧版升级**，双击打开新版配置器，点 **保存更改** 即可更新启动器。[直接下载 v1.2.3 配置器](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.3/ChatGPTLauncherSetup.exe)

如果开启了 **应用于 Codex 环境变量**，请先保存 ChatGPT 和 Codex 中的工作。保存设置后，配置器会尝试退出 ChatGPT。

## ✨ 更新内容

- Windows x64 配置器 `ChatGPTLauncherSetup.exe` 的下载体积比 v1.2.2 正式版小约 7.4%。
- 使用云端 Codex 代理启动 ChatGPT 时，启动器合并代理设置和已有环境变量的速度更快。
- 配置器首次打开、读取设置、校验启动参数或首次加载附加程序列表时，如果等待较久，会在对应的位置显示加载骨架。刷新附加程序时，已有列表仍然可见。

## 🛠️ 修复内容

- 兼容界面的代理端口输入框会按字体调整宽度，英文提示「Auto-detect」现在能完整显示。
- 设置读取较慢、随后参数校验很快完成时，启动参数预览直接显示结果，避免占位条一闪而过。
- 调整了保存完成弹窗的间距，更容易分辨后续步骤和操作按钮。

## ⚠️ 已知问题

- **静默启动** 暂不可用，启动时仍可能看到 ChatGPT 主窗口。
- 如果配置器提示 ChatGPT 仍在运行，请手动完全退出，再从 **ChatGPT Launcher** 快捷方式打开。

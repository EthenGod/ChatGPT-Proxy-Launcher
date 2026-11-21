[简体中文](release-upload.md) | [English](release-upload.en.md) | [日本語](release-upload.ja.md)

# v1.2.2 云端 Codex 代理与启动参数预览

[![快速开始：查看分步教程](https://img.shields.io/badge/%E7%AC%AC%E4%B8%80%E6%AC%A1%E4%BD%BF%E7%94%A8-%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/README.md#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B) [![下载 Windows x64 配置器](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.2/ChatGPTLauncherSetup.exe)

**第一次使用请先看[快速开始](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/README.md#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B)。** 教程中的一行 PowerShell 命令默认下载并打开最新正式版配置器。手动下载和首次设置也有分步说明。

**从旧版升级**，双击打开新版配置器，点 **保存更改** 即可更新启动器。[直接下载 v1.2.2 配置器](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.2/ChatGPTLauncherSetup.exe)

如果开启了 **应用于 Codex 环境变量**，请先保存 ChatGPT 和 Codex 中的工作。保存设置后，配置器会尝试退出 ChatGPT。

## ✨ 更新内容

- 新增 **修复云端 Codex 对话无法连接**，默认开启。配置单个无需认证的 HTTP 或 HTTPS 代理后，请完全退出 ChatGPT，再从配置好的 **ChatGPT Launcher** 快捷方式打开。新进程的云端会话可以使用所选代理。设置了代理绕过规则时保留原有代理环境。
- 主界面的 **启动参数预览** 在参数变化时淡入淡出。左下角提示在窄窗换行时也能完整显示。选择文字或开启「减少动态效果」时，预览立即显示最新内容。
- 等待附加程序就绪时，启动器会提前释放已读取的配置，减少内存占用。

## 🛠️ 修复内容

- 更新已有的 Codex `.env` 文件时，会保留原有文件权限。如果保存失败，配置器会保护恢复所需的文件副本。
- 附加程序缺少依赖时，设置页会标明原因。通过 PowerShell 打开图形配置器时，脚本也会报告配置器的失败结果。

## ⚠️ 已知问题

- **静默启动** 暂不可用，启动时仍可能看到 ChatGPT 主窗口。
- 如果配置器提示 ChatGPT 仍在运行，请手动完全退出，再从 **ChatGPT Launcher** 快捷方式打开。

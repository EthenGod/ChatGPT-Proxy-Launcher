# v1.0.5 配置 Codex 代理并查看启动器版本

[![快速开始：查看分步教程](https://img.shields.io/badge/%E7%AC%AC%E4%B8%80%E6%AC%A1%E4%BD%BF%E7%94%A8-%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B) [![下载 ChatGPTLauncherSetup.exe（Windows x64）](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.0.5/ChatGPTLauncherSetup.exe)

第一次使用？点 **快速开始**，跟着教程用一行 PowerShell 命令打开配置器，或手动下载。按钮不显示时，可点[文字版教程](https://github.com/EthenGod/ChatGPT-Proxy-Launcher#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B)或[直接下载配置器](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.0.5/ChatGPTLauncherSetup.exe)。请先安装 ChatGPT Windows 应用；本工具不是 OpenAI 官方产品。

**从旧版升级：** 打开新版配置器，点击 **保存更改**，即可更新已部署的启动器。

## ✨ 更新内容

- **同时配置 Codex 代理。** 开启“使用指定代理”后，默认同步更新 Codex 的 `.env`，协议、地址和端口跟随界面选择。保存后尝试正常关闭 Codex，请先保存工作，再重新打开 Codex。
- **直接查看版本。** 标题旁显示配置器版本，下方显示已部署的启动器版本。协议和地址旁增加问号提示。
- **改进 PowerShell 下载体验。** 下载时显示进度。配置器退出并清理临时文件后，按任意键或等待 10 秒打开项目主页；直接关闭窗口可取消。

## 🛠️ 修复内容

- 修复 PowerShell 7 中下载进度条换行后残留的问题。
- 遇到不支持的 `.env` 换行时保留原文件并提示，避免误删其他配置。
- 修复 Codex 已退出时仍可能提示“未能确认退出”的问题。

## ⚠️ 已知问题

- **静默启动暂不可用。** “启动时不显示主窗口”已置灰；即使旧配置保留了这个选项，ChatGPT 仍可能显示主窗口。
- **部分情况下需手动退出 Codex。** 如果关闭请求只让窗口缩到托盘，或无法确认进程退出，配置器会提醒手动退出后重新打开。

<!-- 发布标签：v1.0.5。运行 release.cmd 后上传 Releases/v1.0.5/ 中的四个附件。此说明复制到 Release 正文，不作为附件上传。 -->

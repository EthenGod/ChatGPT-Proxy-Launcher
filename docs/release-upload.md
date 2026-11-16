[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.ja.md)

# v1.1.3 桌面快捷方式显示修复

[![快速开始：查看分步教程](https://img.shields.io/badge/%E7%AC%AC%E4%B8%80%E6%AC%A1%E4%BD%BF%E7%94%A8-%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B) [![下载 ChatGPTLauncherSetup.exe（Windows x64）](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.3/ChatGPTLauncherSetup.exe)

第一次使用：点 **快速开始**，跟着教程用一行 PowerShell 命令打开配置器，或手动下载。按钮不显示时，可点[文字版教程](https://github.com/EthenGod/ChatGPT-Proxy-Launcher#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B)或[直接下载配置器](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.3/ChatGPTLauncherSetup.exe)。请先安装 ChatGPT Windows 应用；本工具不是 OpenAI 官方产品。

**从旧版升级：** 打开新版配置器，点击 **保存更改**，即可更新已部署的启动器。

**开启 Codex 的 .env 配置时：** 请先保存工作。保存成功后会尝试退出整个 ChatGPT 桌面应用，请根据底部提示重新打开 ChatGPT。

## ✨ 更新内容

- 创建、更新或移除桌面快捷方式后，配置器会主动通知 Windows 刷新桌面。

## 🛠️ 修复内容

- 修复快捷方式文件已在桌面目录中，但图标需要按 F5 才出现的问题。
- 迁移旧名称快捷方式或保存失败回滚后，同步刷新桌面显示。

## ⚠️ 已知问题

- **静默启动暂不可用。** “启动时不显示主窗口”已置灰；即使旧配置保留了这个选项，ChatGPT 仍可能显示主窗口。
- **部分情况下仍需手动退出 ChatGPT。** 应用未响应退出、窗口无法激活或后台仍在运行时，配置器会提醒手动退出后重新打开，不会强制结束进程。

<!-- 发布标签：v1.1.3。运行 release.cmd 后上传 Releases/v1.1.3/ 中的四个附件。此说明复制到 Release 正文，不作为附件上传。 -->

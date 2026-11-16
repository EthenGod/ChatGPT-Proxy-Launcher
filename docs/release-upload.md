[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.ja.md)

# v1.1.2 稳定性与扩展接入改进

[![快速开始：查看分步教程](https://img.shields.io/badge/%E7%AC%AC%E4%B8%80%E6%AC%A1%E4%BD%BF%E7%94%A8-%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B) [![下载 ChatGPTLauncherSetup.exe（Windows x64）](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.2/ChatGPTLauncherSetup.exe)

第一次使用：点 **快速开始**，跟着教程用一行 PowerShell 命令打开配置器，或手动下载。按钮不显示时，可点[文字版教程](https://github.com/EthenGod/ChatGPT-Proxy-Launcher#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B)或[直接下载配置器](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.2/ChatGPTLauncherSetup.exe)。请先安装 ChatGPT Windows 应用；本工具不是 OpenAI 官方产品。

**从旧版升级：** 打开新版配置器，点击 **保存更改**，即可更新已部署的启动器。

**开启 Codex 的 .env 配置时：** 请先保存工作。保存成功后会尝试退出整个 ChatGPT 桌面应用，请根据底部提示重新打开 ChatGPT。

## ✨ 更新内容

- 桌面和自启动快捷方式统一命名为 **ChatGPT Launcher**。保存设置时，会迁移本工具创建的旧名称快捷方式。
- PowerShell 后台入口支持添加、配置、停用、移除和查询链式启动扩展，方便第三方程序接入。
- 完善[开发者接入指南](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.1.2/docs/developer-guide.md)和示例，说明启动器发现、按需后台部署、依赖和失败处理。

## 🛠️ 修复内容

- 改进部署中断和内存不足时的回滚；无法完全恢复时会明确提示。
- 移除快捷方式前重新核对内容；回滚恢复已移除的快捷方式时，不覆盖新出现的同名文件。
- 减少启动器和配置器的临时内存分配，及时释放链式启动等待期间不再需要的缓冲区和句柄。
- 主题偏好保存失败时，保留退出并重新打开 ChatGPT 的提醒。

## ⚠️ 已知问题

- **静默启动暂不可用。** “启动时不显示主窗口”已置灰；即使旧配置保留了这个选项，ChatGPT 仍可能显示主窗口。
- **部分情况下仍需手动退出 ChatGPT。** 应用未响应退出、窗口无法激活或后台仍在运行时，配置器会提醒手动退出后重新打开，不会强制结束进程。

<!-- 发布标签：v1.1.2。运行 release.cmd 后上传 Releases/v1.1.2/ 中的四个附件。此说明复制到 Release 正文，不作为附件上传。 -->

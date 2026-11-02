# v1.0.4 启动 ChatGPT 时也能打开插件

[![快速开始：查看分步教程](https://img.shields.io/badge/%E7%AC%AC%E4%B8%80%E6%AC%A1%E4%BD%BF%E7%94%A8-%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B) [![下载 ChatGPTLauncherSetup.exe（Windows x64）](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.0.4/ChatGPTLauncherSetup.exe)

第一次使用？点 **快速开始**，跟着教程用一行 PowerShell 命令打开配置器，或手动下载。按钮不显示时，可点[文字版教程](https://github.com/EthenGod/ChatGPT-Proxy-Launcher#%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B)或[直接下载配置器](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.0.4/ChatGPTLauncherSetup.exe)。请先安装 ChatGPT Windows 应用；本工具不是 OpenAI 官方产品。

**从旧版升级：** 打开新版配置器，点击 **保存更改**，即可更新已部署的启动器。

## ✨ 更新内容

- **启动 ChatGPT 时一起打开插件。** 在 **高级 → 添加程序…** 中选择插件入口，通过本工具启动 ChatGPT 时就会一并打开。要取消联动，点击 **打开启动目录**，移出对应入口即可。
- **支持多种插件入口。** 可选择 EXE、快捷方式、批处理、PowerShell、VBS、JScript、WSF、Python、Node.js 模块和可执行 JAR。Python、Node.js 和 Java 插件需先安装对应的运行环境。
- **一行命令打开配置器。** 复制“快速开始”中的 PowerShell 命令即可下载最新版。校验通过后自动打开，退出配置器后清理临时下载文件。

## 🛠️ 修复内容

- 修复部署时可能覆盖同名文件的问题。如果检查后、保存前出现了同名文件，配置器会保留原文件并提示冲突。
- 修复插件脚本移走后，旧快捷方式仍会打开脚本运行程序的问题。现在会跳过失效入口，继续启动其他插件。
- 修复无法开始保存时按钮可能无法再次点击的问题。设置会保留，可以直接重试。

## ⚠️ 已知问题

- **静默启动暂不可用。** “启动时不显示主窗口”已置灰；即使旧配置保留了这个选项，ChatGPT 仍可能显示主窗口。

<!-- 发布标签：v1.0.4。上传 Releases/v1.0.4/ 中的四个附件。此说明复制到 Release 正文，不作为附件上传。 -->

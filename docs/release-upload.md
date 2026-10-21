# v1.0.1 · 任务栏图标修复与项目整理

非官方 Windows x64 ChatGPT 启动器，提供图形化配置、登录自启和代理参数设置。使用前请先安装 ChatGPT Windows 应用。

## 📥 下载与使用

**第一次使用？只需下载下面这一个文件：`ChatGPTLauncherSetup.exe`。**

[![下载 ChatGPTLauncherSetup.exe（Windows x64，推荐）](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563eb?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.0.1/ChatGPTLauncherSetup.exe)

[按钮无法显示？点这里直接下载 ChatGPTLauncherSetup.exe](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.0.1/ChatGPTLauncherSetup.exe)

1. 下载后双击 `ChatGPTLauncherSetup.exe`，打开配置界面。
2. 按需要设置登录自启和代理；不需要代理时，不勾选“使用指定代理”。
3. 点击 **“部署并保存”**，按提示核对 Windows 自启设置，完成后关闭工具。

部署工具已内置启动器，**不用再下载另一个 EXE，也不用下载源码包**，无需额外安装 .NET 或 WebView2。保存配置不会立即启动 ChatGPT。

**旧版本用户：**重新运行新版部署工具并保存，即可更新已部署的启动器和快捷方式。

## ✨ 更新内容

- 全部测试源码和测试入口统一放入 `tests/`，正式源码与本机临时文件分开管理。
- 同步源码打包清单、构建说明及版本信息，精简过时的验证记录。
- 增加任务栏快捷方式修复的隔离测试，并验证源码包可在全新目录构建和测试。

## 🛠 修复内容

- **修复一类 ChatGPT 更新后，运行中任务栏图标变成空白的问题。** 原因是开始菜单快捷方式仍指向旧版本目录。新版启动器会在正常启动前检查已确认身份的快捷方式，将目标与图标关联到稳定的启动器路径。
- 修复时保留原有参数和应用标识，首次修改前备份原件；已经正确关联时不会重复写入。修复失败也不会阻止 ChatGPT 正常启动。

更新后，请通过新版启动器启动 ChatGPT，让检查和修复生效；仅打开配置工具或运行检查模式不会触发这项修复。

## ⚠️ 已知问题与限制

- **静默启动可能不生效。** “启动时不显示主窗口”的实际效果取决于 ChatGPT 版本，仍可能显示主窗口；保存成功不代表静默启动已验证。
- **部分图标问题仍需手动处理。** 独立的任务栏固定项、手动修改过的快捷方式，以及官方改变快捷方式名称或应用标识的情况，不在自动修复范围内。图标显示异常时，可先核对快捷方式指向。
- **代理端口不一定能自动识别。** 不支持的配置、远程地址、IPv6 或多个候选结果需要手动填写；检测到端口不代表已联网，代理参数也不保证覆盖语音和所有子工具流量。
- **ChatGPT 自带自启不一定能自动关闭。** 遇到系统版本、权限或状态差异时，请按提示进入 Windows“设置 → 应用 → 启动”手动核对，避免两个入口同时启动。

## 📦 其他附件怎么选

| 文件 | 适合谁 |
| --- | --- |
| **`ChatGPTLauncherSetup.exe`** | **推荐：普通用户，只下载这个即可完成图形化部署和配置** |
| `ChatGPTProxyLauncher.exe` | 熟悉快捷方式参数、想手动配置的用户 |
| `ChatGPTLauncher-source.zip` | 想查看代码或自行编译的开发者 |
| `SHA256SUMS.txt` | 需要核对下载文件 SHA-256 的用户 |

GitHub 自动提供的 **Source code (zip / tar.gz)** 也是源码，不是安装程序。

本版本通过构建、六组隔离测试及源码包全新目录复测。实际登录自启、商店更新、静默效果和联网仍需在使用环境中验证。

<!--
维护者发布提示（不会显示在 Release 正文中）：
本文件可直接复制到 GitHub Release 正文；release.cmd 会同步到 Releases/README.md。
发布标签使用 v1.0.1，上传 Releases/v1.0.1/ 内两个 EXE、ChatGPTLauncher-source.zip、SHA256SUMS.txt，共四个附件。
下载按钮仅在该标签的 Release 已发布且同名附件上传完成后生效；修改版本时同步更新标题、按钮和备用链接。
Releases/README.md 无需作为附件上传。生成附件不会自动发布到 GitHub。
-->

# ChatGPT 轻量启动器

用于 Windows x64 的非官方 ChatGPT 启动器。它可以按指定的代理设置启动已安装的 ChatGPT，也可以创建桌面快捷方式或设置登录自启。启动器发出启动请求后即退出，不在后台常驻，不修改系统代理。

[![前往 GitHub Releases 下载 Windows x64 版](https://img.shields.io/badge/Windows%20x64-%E5%89%8D%E5%BE%80%20Releases%20%E4%B8%8B%E8%BD%BD-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases)

打开 [GitHub Releases 页面](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases)，在最新版本的 **Assets（附件）** 中下载 **`ChatGPTLauncherSetup.exe`**。它已内置启动器，普通用户只需下载这一个文件。使用前请先安装 ChatGPT Windows 应用。

## 快速开始

1. 双击 `ChatGPTLauncherSetup.exe`。按需勾选“登录 Windows 后自动启动”或“创建桌面快捷方式”。
2. 需要代理时，勾选“使用指定代理”，选择协议并填写地址。先运行代理软件，再点击空白的端口框，工具会尝试填入本机端口。未检测到时，请查看代理软件的端口设置并手动填写。
3. 核对启动参数预览，点击“部署并保存”。如果出现自启提示，按提示到 Windows 的“设置 → 应用 → 启动”核对，避免 ChatGPT 自带自启和本工具同时启动。
4. 保存后可以关闭配置工具。若已创建桌面快捷方式，双击它即可按当前配置启动 ChatGPT。保存配置本身不会启动 ChatGPT。

以后要修改配置，重新打开部署工具，调整后点击“保存更改”。默认部署目录为 `%LOCALAPPDATA%\ChatGPTProxyLauncher`；在“高级”中可更改目录、设置代理绕过列表或添加其他启动参数。

## 使用说明

- **代理端口：** 自动检测限于本机地址 `127.0.0.1` 或 `localhost` 的 HTTP、SOCKS5 代理。可识别 Clash Verge Rev、FlClash、Clash for Windows 和 v2rayN 的常见配置。检测到端口不代表联网成功。若 ChatGPT 已在运行，请完整退出后再通过启动器启动，以使用新的启动参数。
- **桌面快捷方式：** 默认不创建。开启后，保存配置会同步更新快捷方式的目标和参数；关闭后再保存，只移除本工具管理的桌面快捷方式。
- **静默启动：** “启动时不显示主窗口”暂不可用，开关已置灰。旧配置中的勾选状态会保留，但不能保证隐藏主窗口。
- **任务栏图标：** ChatGPT 更新后，如果图标空白，可通过本启动器启动一次。它会检查开始菜单快捷方式，确认属于 ChatGPT 才尝试修复；独立固定到任务栏的旧快捷方式不在修复范围内。详见[任务栏图标修复说明](docs/taskbar-repair.md)。

## 命令行用法

如需手动配置，可从同一发布页下载独立的 `ChatGPTProxyLauncher.exe`。以下示例使用本机 HTTP 代理，请将端口换成代理软件显示的值：

```cmd
ChatGPTProxyLauncher.exe --proxy-server=http://127.0.0.1:7897
```

在命令提示符中，将 `--launcher-check` 放在第一个参数位置，可查看程序路径和待传参数。此模式不会启动 ChatGPT 或修改快捷方式：

```cmd
ChatGPTProxyLauncher.exe --launcher-check --proxy-server=http://127.0.0.1:7897 > check.txt
```

`check.txt` 含实际路径和完整参数，分享前请移除个人路径、私人代理地址和凭据。

## 从源码构建

需要 Visual Studio 2022 Build Tools 的 C++ 工具和 Windows SDK。在项目根目录运行：

| 命令 | 用途 |
| --- | --- |
| `cmd /c build.cmd` | 构建启动器和图形部署工具，产物在 `outputs/`。 |
| `cmd /c tests\test-build.cmd` | 重新构建并运行隔离测试，不启动 ChatGPT，不修改真实自启项。 |
| `cmd /c release.cmd` | 构建、测试并生成本地发布附件，存入 `Releases/v版本号/`。不会上传到 GitHub。 |

## 许可证

本项目代码使用 [GNU GPL 第 3 版](LICENSE)（仅此版本，`GPL-3.0-only`）。ChatGPT 名称和图标的相关权利归 OpenAI 所有，图标不在本项目代码许可证的授权范围内。详见[图标来源与权利说明](assets/README.md)。

# ChatGPT 轻量启动器

用于 Windows x64 的非官方 ChatGPT 启动器。支持为 ChatGPT 指定代理、创建桌面快捷方式和设置登录自启。启动后即退出，不修改系统代理。

## 快速开始

**开始前：**按键盘上的 **Windows 徽标键（Win）**，输入 `ChatGPT`。如果能找到并打开 ChatGPT 桌面应用，就可以继续。如果平时只在浏览器里用 ChatGPT，请先从 [ChatGPT 官方下载页](https://chatgpt.com/download/) 安装 Windows 版。本工具只负责设置启动方式。

下面两种方式打开的是同一个配置器。**选一种就行，推荐方式一。**

### 方式一：用 PowerShell 一行打开（推荐）

1. 按 **Win**，输入 `Windows PowerShell`，点击搜索结果中的 **Windows PowerShell**。用系统自带的版本即可；如果想用 PowerShell 7，可从 [PowerShell 7 官方发布页](https://github.com/PowerShell/PowerShell/releases/latest) 下载。
2. 点下面代码框右上角的复制按钮。如果没看到按钮，用鼠标从 `irm` 选到最后的 `iex`，按 **Ctrl+C** 复制。

   ```powershell
   irm https://raw.githubusercontent.com/EthenGod/ChatGPT-Proxy-Launcher/master/setup.ps1 | iex
   ```

3. 点击刚打开的 PowerShell 窗口，按 **Ctrl+V** 粘贴，再按 **Enter（回车）** 运行。
4. 看到 `Downloading ChatGPT Launcher Setup` 表示正在下载。等 **“ChatGPT 启动器设置”** 窗口出现，继续看下面的 **“打开后怎么设置”**。先别关 PowerShell，配置完成后再关。

如果出现报错而没有打开配置器，可以改用方式二。

### 方式二：手动下载并打开

[![打开 ChatGPT 轻量启动器的最新版下载页](https://img.shields.io/badge/GitHub-%E6%89%93%E5%BC%80%E6%9C%80%E6%96%B0%E7%89%88%E4%B8%8B%E8%BD%BD%E9%A1%B5-2563EB?style=for-the-badge&logo=github)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/latest)

1. 点击上方蓝色按钮（或[点这里打开下载页](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/latest)）。向下翻到 **Assets（附件）**；如果没看到文件列表，点一下 **Assets** 展开。
2. 点击 **`ChatGPTLauncherSetup.exe`** 下载。只需这一个文件，不用下载另一个 EXE 或源码 ZIP。
3. 等下载完成，在浏览器按 **Ctrl+J** 打开下载记录。找到 `ChatGPTLauncherSetup.exe`，点击旁边的文件夹图标或“在文件夹中显示”。
4. 在打开的文件夹里双击刚下载的文件。看到 **“ChatGPT 启动器设置”** 窗口后，按下面的步骤设置。

### 打开后怎么设置（两种方式相同）

1. **选择什么时候启动。** 找到 **登录 Windows 后自动启动**。想让 ChatGPT 每次登录电脑时打开，就保持开启；只想自己手动打开，就点击右侧开关把它关闭。
2. **在桌面放一个启动图标。** 打开 **创建桌面快捷方式**，方便稍后双击启动。不想创建就保持关闭。下面灰色的 **启动时不显示主窗口** 暂不可用，跳过即可。
3. **设置代理。** 需要代理时：
   - 先打开平时用的代理软件，让它保持运行，再回到配置器，打开 **使用指定代理**。
   - 在 **协议** 中选择代理软件提供的类型。例如，软件显示“HTTP 端口”，就选 **HTTP**。如果代理软件就在这台电脑上，**地址** 保留默认的 `127.0.0.1`。
   - 点击空白的 **端口** 输入框。如果自动出现数字，就不用再填。如果仍为空，打开代理软件的设置，找到所选协议对应的端口号，把那串数字填回这里。
4. **保存设置。** 下方的 **启动参数预览** 会自动更新，不用在里面打字。第一次使用可以跳过“高级”，直接点右下角 **部署并保存**。如果以前部署过，按钮叫 **保存更改**。
5. **如果弹出“部署已完成”：**点 **确定** 打开 Windows 的“启动应用”设置。找到 **ChatGPT**，确认它自带的启动开关为**关**；找到 **ChatGPT Proxy Launcher**，确认它的开关为**开**。这样登录时才会使用刚保存的设置。
6. **关闭配置器。** 回到配置器，看到 **已保存** 后点 **完成**，不用再部署一次。用方式一的用户，请等 PowerShell 再次显示 `PS …>` 输入提示后再关窗口，让脚本先清理临时下载文件。
7. **试着启动。** 保存设置不会立即打开 ChatGPT。如果创建了桌面图标，回到桌面双击 **ChatGPT Proxy Launcher**。如果 ChatGPT 已在运行，先完整退出它，再双击图标。如果只开启了登录自启，下次登录 Windows 时会自动尝试启动。

**以后想改设置：**用方式一重新运行命令，或用方式二再次双击下载好的配置器。改完点 **保存更改** 即可。

## 使用说明

### 启动 ChatGPT 时一起打开插件或其他程序

1. 打开新版配置器，点左下角 **高级**，找到 **启动 ChatGPT 后打开**。
2. 点 **添加程序…**，选中插件或其他程序的 `.exe`，点 **打开**。配置器会立即在专用目录创建快捷方式，原程序留在原处。
3. 第一次使用或从旧版升级时，点 **返回 → 部署并保存**（已部署过则点 **保存更改**）。之后通过本工具的桌面快捷方式或登录自启打开 ChatGPT，就会一起打开附加程序。从 ChatGPT 官方图标启动不会触发。
4. 要查看或取消联动，点 **高级 → 打开启动目录**。把对应快捷方式移出目录即可。也可以把已有的程序快捷方式放进去；要带参数时，在快捷方式属性里设置。支持直接放入可独立运行的 EXE，不要单独搬走依赖其他文件的插件主程序。

启动目录是 `%LOCALAPPDATA%\ChatGPTProxyLauncher\StartupApps`，不是 Windows 自启文件夹。只处理目录第一层的本机 EXE 和指向本机 EXE 的普通快捷方式。每次通过启动器成功创建 ChatGPT 进程后都会尝试启动这些程序，不等 ChatGPT 窗口或插件加载完成，也不检查插件是否已经运行。

### 其他设置

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

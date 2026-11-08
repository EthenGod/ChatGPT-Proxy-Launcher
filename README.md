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
4. PowerShell 会显示获取版本、下载和校验的进度。等 **“ChatGPT 启动器设置”** 窗口出现，继续看下面的 **“打开后怎么设置”**。先别关 PowerShell，退出配置器后会自动清理并关闭。

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
   - **同时配置 Codex 的 .env 文件（推荐）** 默认开启。它会使用上面相同的协议、地址和端口。鼠标停在旁边的 **?** 上可查看说明；不需要时关闭这个开关。
4. **保存设置。** 下方的 **启动参数预览** 会自动更新，不用在里面打字。第一次使用可以跳过“高级”，直接点右下角 **部署并保存**。如果以前部署过，按钮叫 **保存更改**。
5. **如果弹出“部署已完成”：**点 **确定** 打开 Windows 的“启动应用”设置。找到 **ChatGPT**，确认它自带的启动开关为**关**；找到 **ChatGPT Proxy Launcher**，确认它的开关为**开**。这样登录时才会使用刚保存的设置。
6. **关闭配置器。** 回到配置器，看到 **已保存** 后点 **完成**，不用再部署一次。用方式一的用户，PowerShell 会先清理临时下载文件，再显示 Star 提示。按任意键会立即打开本项目主页并退出；不操作则在 10 秒后自动打开并退出。只想退出、不打开网页时，直接关闭 PowerShell 窗口。若下载、校验或清理失败，会保留窗口显示错误。
7. **试着启动。** 保存设置不会立即打开 ChatGPT。如果创建了桌面图标，回到桌面双击 **ChatGPT Proxy Launcher**。如果 ChatGPT 已在运行，先完整退出它，再双击图标。如果只开启了登录自启，下次登录 Windows 时会自动尝试启动。

**以后想改设置：**用方式一重新运行命令，或用方式二再次双击下载好的配置器。改完点 **保存更改** 即可。

**如果开启了 Codex 的 .env 配置：**先保存 ChatGPT 和 Codex 中的工作。配置器保存成功后会尝试退出整个 ChatGPT 桌面应用，最多等待 10 秒。请根据底部提示重新打开 ChatGPT；如果提示未能确认已退出，请先手动完全退出。配置器会在 `%USERPROFILE%\.codex\.env` 中更新 `all_proxy`、`ALL_PROXY`、`HTTP_PROXY` 和 `HTTPS_PROXY`，缺少的变量会补上，其他内容保留。这项设置独立于启动器参数，不作用于 Chat。关闭选项后再保存，只停止更新，不删除已写入的内容；更换代理端口时请重新保存。它可能有助于解决 Codex 的重连问题，但不保证联网成功。

## 使用说明

### 启动 ChatGPT 时一起打开插件或其他程序

1. 打开新版配置器，点左下角 **高级**，找到 **启动 ChatGPT 后打开**。
2. 点 **添加程序…**，选中插件平时用来启动的文件（支持类型见下表），再点 **打开**。配置器会为它创建快捷方式；如果选的是已有快捷方式，就复制一份。原文件不移动，同名文件不会被覆盖。
3. 第一次使用或从旧版升级时，点 **返回 → 部署并保存**（已部署过则点 **保存更改**）。之后通过本工具的桌面快捷方式或登录自启打开 ChatGPT，就会一起打开附加程序。从 ChatGPT 官方图标启动不会触发。
4. 要查看或取消联动，点 **高级 → 打开启动目录**。把对应入口移出目录即可。也可以直接放入上述类型的文件；如果插件依赖其他文件，建议添加快捷方式，保留插件原来的位置。

启动目录是 `%LOCALAPPDATA%\ChatGPTProxyLauncher\StartupApps`，不是 Windows 自启文件夹。只处理第一层的本机程序、脚本，以及指向它们的普通快捷方式。每次通过启动器成功创建 ChatGPT 进程后都会尝试启动这些入口，不等窗口加载，也不检查插件是否已经运行。

| 选择的文件 | 打开方式 |
| --- | --- |
| `.exe`、`.lnk` | 直接启动程序，或使用指向本表中程序、脚本的普通快捷方式。 |
| `.bat`、`.cmd`、`.ps1` | 批处理使用 cmd；PS1 使用 Windows PowerShell，遵循本机执行策略。批处理需要的参数请写在文件内。 |
| `.vbs`、`.js`、`.wsf` | 使用 Windows Script Host。电脑需启用相应脚本引擎。这里的 JS 是传统 Windows JScript。 |
| `.py`、`.pyw` | 需要 Python。`.py` 使用 `python.exe`，无控制台脚本 `.pyw` 使用 `pythonw.exe`。 |
| `.cjs`、`.mjs` | 需要 Node.js（`node.exe`）。若插件用 Node.js 运行普通 `.js`，请选择它原有的 Node.js 启动快捷方式。 |
| `.jar` | 需要 Java（`java.exe`），只适合本身能用 `java -jar` 启动的程序包。 |

提示找不到 Python、Node.js 或 Java 时，请确认对应程序已安装、其目录在 Windows 的 `PATH` 中，然后重新打开配置器。也可以选择已有快捷方式，让它直接指向插件使用的解释器 EXE，并在参数里指定脚本。这也适用于 Python 虚拟环境和需要特定版本的插件。工具不会自动安装运行环境。

### 其他设置

- **版本信息：** 标题旁显示配置器自身版本，标题下显示已部署启动器的版本，打开配置器和保存后自动检查。旧版没有版本信息时显示“版本无法识别”；用新版配置器保存一次后即可读取新版启动器的版本。这里显示的是本机部署版本，不是联网检查最新版本。
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

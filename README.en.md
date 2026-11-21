[简体中文](README.md) | [English](README.en.md) | [日本語](README.ja.md)

> This document may include machine-translated content. In case of discrepancies, the [Chinese original](README.md) takes precedence.

# ChatGPT Lightweight Launcher

An unofficial launcher for ChatGPT on Windows x64. Choose a proxy for ChatGPT, create a desktop shortcut, and start it when you sign in. The launcher exits after starting the app and does not change your system proxy.

## Quick start

[![Xiaoheihe | Short tutorial in Chinese](https://img.shields.io/badge/Xiaoheihe-Tutorial_in_Chinese-1F2328?style=for-the-badge)](https://www.xiaoheihe.cn/app/bbs/link/196448409)

**Before you begin:** press the **Windows logo key (Win)** and type `ChatGPT`. If you can find and open the ChatGPT desktop app, continue below. If you only use ChatGPT in a browser, first install the Windows app from the [official ChatGPT download page](https://chatgpt.com/download/). This tool only configures how the app starts.

Both methods open the same setup tool. **Choose one; method 1 is recommended.**

### Method 1: Open with one PowerShell command (recommended)

1. Press **Win**, type `Windows PowerShell`, and click **Windows PowerShell** in the results. The version included with Windows is sufficient. If you prefer PowerShell 7, download it from the [official PowerShell releases page](https://github.com/PowerShell/PowerShell/releases/latest).
2. Click the copy button at the top right of the code block below. If there is no button, select everything from `irm` through the final `iex`, then press **Ctrl+C**.

   ```powershell
   irm https://raw.githubusercontent.com/EthenGod/ChatGPT-Proxy-Launcher/master/setup.ps1 | iex
   ```

3. Click the PowerShell window, press **Ctrl+V** to paste, then press **Enter**.

   If PowerShell says WebView2 is missing, type **1** and press **Enter** to install it (recommended). To continue in compatibility mode, type **2** and press **Enter**.

4. PowerShell shows progress while finding the release, downloading, and verifying it. When **ChatGPT Launcher** opens, follow **Configure the launcher** below. Leave PowerShell open; after the setup tool exits, it cleans up the downloads and shows the completion prompt.

If an error appears and the setup tool does not open, use method 2.

### Method 2: Download and open manually

[![GitHub | Open latest downloads](https://img.shields.io/badge/GitHub-Open_latest_downloads-2563EB?style=for-the-badge&logo=github)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/latest)

1. Click the blue button above, or [open the download page here](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/latest). Scroll to **Assets**. If the file list is hidden, click **Assets** to expand it.
2. Click **`ChatGPTLauncherSetup.exe`**. This is the only file you need; you do not need the other EXE or the source ZIP.
3. After the download finishes, press **Ctrl+J** in your browser. Find `ChatGPTLauncherSetup.exe`, then click its folder icon or **Show in folder**.
4. Double-click the downloaded file in that folder. When **ChatGPT Launcher** opens, continue below. If your PC is missing WebView2, the configurator opens in compatibility mode automatically.

### Configure the launcher (same for both methods)

The setup tool follows the Windows display language by default. It supports 简体中文, English, and 日本語, with Chinese as the fallback. Change the language at the top right, or choose **System language**. Your choice is remembered without redeploying.

1. **Choose when to start.** Leave **Start at sign-in** on if you want ChatGPT to open at sign-in. Turn it off if you prefer to open ChatGPT yourself.
2. **Add a desktop icon.** Turn on **Desktop shortcut** if you want an easy way to open ChatGPT later. Otherwise, leave it off. **Silent startup** is disabled for now; skip it.
3. **Set up a proxy**, if needed:
   - Open your usual proxy app and leave it running. Return to the setup tool and turn on the switch beside **Proxy**.
   - Select the protocol offered by your proxy app. For example, select **HTTP** for an HTTP port. If the proxy app runs on this computer, keep **Address** set to `127.0.0.1`.
   - Turning the proxy on detects the port once. You can also click **Detect port** inside the port field. Check the result against your proxy app and correct it if needed. Click the notification at the bottom right to see its source. Detection does not prove connectivity.
   - **Apply to Codex environment variables** is on by default. It writes the same proxy to the Codex `.env` file. Click **?** for details.
   - **Fix connection issues in cloud Codex conversations** is on by default. You can switch it off independently. After saving, start ChatGPT from the configured shortcut. Its Node processes can use the selected HTTP or HTTPS proxy. Use a proxy address without credentials and enter internationalized hostnames in Punycode. The new process inherits `NO_PROXY`. If the launch arguments include any proxy bypass rules, Node keeps its existing proxy environment. Click **?** for details.
4. **Save.** **Launch arguments** updates automatically; you do not need to type there. For a first setup, skip **Advanced settings** and click **Deploy and save** at the bottom right. If already deployed, the button says **Save changes**.
5. **If asked to check sign-in startup after saving:** click **Open startup app settings**. Make sure **ChatGPT** is **Off** and **ChatGPT Launcher** is **On** in Windows. The **Details** button at the bottom reopens this reminder.
6. **Close the setup tool.** Once it shows **Saved**, click **Done**. There is no need to deploy again. With method 1, PowerShell removes the temporary downloads and shows a Star prompt. Press any key to open this project's page and exit, or wait 10 seconds for it to do so automatically. To exit without opening the page, close the PowerShell window. Download, verification, or cleanup failures leave the window open with an error.
7. **Try starting ChatGPT.** Saving does not open ChatGPT. If you created a desktop shortcut, double-click **ChatGPT Launcher** on your desktop. If ChatGPT is already running, fully quit it first. If you only enabled sign-in startup, it will try to open ChatGPT the next time you sign in.

Older shortcuts may still be named **ChatGPT Proxy Launcher**. Saving with the updated setup tool migrates the shortcuts it manages to the new name.

**To change settings later:** run method 1 again, or open the downloaded setup tool from method 2. Make your changes and click **Save changes**.

If setup is already open, running it again shows the existing window. If you opened it in another Windows session under the same account, switch back to that session to continue.

**If Codex .env configuration is on:** save your work in ChatGPT and Codex first. After saving, the setup tool tries to quit the entire ChatGPT desktop app and waits up to 10 seconds. Follow the message at the bottom to reopen it. If the tool cannot confirm that ChatGPT has fully quit, close it completely yourself first. The tool updates `all_proxy`, `ALL_PROXY`, `HTTP_PROXY`, and `HTTPS_PROXY` in `%USERPROFILE%\.codex\.env`, adds missing variables, and preserves other content. This configuration is independent of launcher arguments and does not affect Chat. Turning the option off stops future updates; it does not remove existing entries. Save again after changing your proxy port. This may help with Codex reconnecting, but does not guarantee connectivity.

## Usage

To add or improve a translation, see the [community translation guide](docs/translating.en.md). Only language files need editing; no C++ changes are required.

### Open plugins or other programs after ChatGPT

1. Open the setup tool and expand **Advanced settings**. **Companion programs** lists entries in the startup folder and registered extensions. **Protocol v1** means the extension has a valid manifest. Click an entry to view its full name, path and launch arguments. Click Refresh after adding or removing files in the folder.
2. Click **Add program…**, select the file you normally use to start the plugin, and click **Open**. Supported types are listed below. The tool creates a shortcut, or copies the file if it is already a shortcut. It does not move the original or overwrite an existing file of the same name.
3. Adding a program takes effect immediately. For first use or an upgrade, also click **Deploy and save** at the bottom, or **Save changes** if already deployed. Programs then open when ChatGPT starts through this tool's desktop shortcut or sign-in startup. Opening ChatGPT from its official icon does not trigger them.
4. To remove an ordinary program, click the **Open startup folder** icon beside the add button and move the entry out of the folder. You may also put supported files there directly. If a plugin needs other files, add a shortcut so its original folder stays intact. Use the extension’s installer to manage a registered extension.

This folder is `%LOCALAPPDATA%\ChatGPTProxyLauncher\StartupApps`, separate from the Windows Startup folder. Only local programs, scripts, and ordinary shortcuts directly inside it are processed. Each successful creation of a ChatGPT process triggers these entries; the launcher does not wait for ChatGPT's window or check whether a legacy companion is already running.

| File | How it opens |
| --- | --- |
| `.exe`, `.lnk` | Starts the program directly, or follows an ordinary shortcut to a supported program or script. |
| `.bat`, `.cmd`, `.ps1` | Uses cmd for batch files and Windows PowerShell for PS1, following the computer's execution policy. Put batch arguments inside the script. |
| `.vbs`, `.js`, `.wsf` | Uses Windows Script Host, which must be enabled. JS here means traditional Windows JScript. |
| `.py`, `.pyw` | Requires Python. Uses `python.exe` for `.py` and `pythonw.exe` for `.pyw` without a console. |
| `.cjs`, `.mjs` | Requires Node.js (`node.exe`). For a Node.js plugin whose entry is `.js`, select its existing Node.js launch shortcut. |
| `.jar` | Requires Java (`java.exe`); only archives that work with `java -jar` are supported. |

If Python, Node.js, or Java is not found, make sure it is installed and its folder is in Windows `PATH`, then reopen the setup tool. You can also select a shortcut that points directly to the interpreter EXE and passes the script as an argument. This supports Python virtual environments and plugins needing a specific runtime version. The tool does not install runtimes.

### Other settings

- **Versions:** the title shows the setup tool's version, and the line below shows the deployed launcher's version. These are checked on opening and after saving. Old launchers without version information show an unavailable version; save with the current setup tool to deploy a version that can be read. This checks the local deployment, not the latest online release.
- **Proxy detection:** supports local HTTP and SOCKS5 proxies at `127.0.0.1` or `localhost`, including common configurations from Clash Verge Rev, FlClash, Clash for Windows, and v2rayN. Detecting a port does not prove connectivity. Fully quit any running ChatGPT instance before opening it through the launcher to apply new launch options.
- **Desktop shortcut:** off by default. Saving with it on updates the target and arguments. Turning it off and saving removes only the desktop shortcut managed by this tool.
- **Silent startup:** unavailable for now. The switch is disabled. Previously saved selections are preserved, but cannot guarantee that the main window stays hidden.
- **Taskbar icon:** if the icon is blank after a ChatGPT update, try opening ChatGPT through this launcher once. It checks the Start menu shortcut and only attempts repair after verifying that it belongs to ChatGPT. Separately pinned taskbar shortcuts are outside its scope. See [taskbar icon repair](docs/taskbar-repair.en.md).

## Command line

For manual use, download the standalone `ChatGPTProxyLauncher.exe` from the same release page. Replace the example port below with the one shown by your proxy app:

```cmd
ChatGPTProxyLauncher.exe --proxy-server=http://127.0.0.1:7897
```

In Command Prompt, put `--launcher-check` first to inspect the application path and forwarded arguments. This mode does not start ChatGPT or modify shortcuts:

```cmd
ChatGPTProxyLauncher.exe --launcher-check --proxy-server=http://127.0.0.1:7897 > check.txt
```

`check.txt` contains actual paths and complete arguments. Remove personal paths, private proxy addresses, and credentials before sharing it.

## Build from source

**For script and plugin authors:** v1.1.0 adds headless deployment, deployment queries, and companion startup with readiness checks and limited retries. Start with the [developer guide (Chinese)](docs/developer-guide.md) and [automation usage (Chinese)](docs/automation-usage.md). Online commands require the matching script and EXE to be published on GitHub. Other users can follow the setup steps above.

A full build requires the Visual Studio 2022 Build Tools C++ tools, Windows SDK, Node.js (24.x starting at 24.15, or 26 and later), and a Rust 1.99 or newer MSVC toolchain with Clippy and rustfmt. Install the locked frontend dependencies first:

```powershell
cd frontend
npm ci
cd ..
```

Then run these commands from the project root. Building only the standalone launcher or old GUI does not require Node.js or Rust. If Rust is not on PATH, see [toolchain configuration (Chinese)](docs/ui-refactor.md#开发命令).

| Command | Purpose |
| --- | --- |
| `cmd /c build.cmd` | Builds both EXEs in `outputs/`. |
| `cmd /c build.cmd native` | Builds the old GUI in `work/ui-native/` without replacing the default setup tool. |
| `cmd /c build.cmd launcher` | Builds only the standalone launcher in `outputs/`. |
| `cmd /c tests\test-build.cmd` | Rebuilds and runs isolated tests without starting ChatGPT or changing real startup entries. |
| `cmd /c tests\analyze.cmd` | Runs C++ static analysis, TypeScript, Rust formatting checks, and Clippy. |
| `cmd /c release.cmd` | Builds, tests, and creates local upload files in `Releases/v<version>/`. Does not upload to GitHub. |

## License

The code is licensed under [GNU GPL version 3](LICENSE), version 3 only (`GPL-3.0-only`). Rights to the ChatGPT name and icon belong to OpenAI; the icon is not covered by the code license. See [icon source and rights](assets/README.en.md).

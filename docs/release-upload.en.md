[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.ja.md)

> This document may include machine-translated content. In case of discrepancies, the [Chinese original](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.md) takes precedence.

# v1.1.2 Stability and companion integration improvements

[![Quick start: step-by-step instructions](https://img.shields.io/badge/First_time-Quick_start-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/README.en.md#quick-start) [![Download ChatGPTLauncherSetup.exe for Windows x64](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.2/ChatGPTLauncherSetup.exe)

First time here? Open **Quick start** and follow the steps to launch the configurator with one PowerShell command or download it manually. If the badges do not appear, use the [text guide](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/README.en.md#quick-start) or [download the configurator directly](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.2/ChatGPTLauncherSetup.exe). Install the ChatGPT Windows app first. This is not an official OpenAI product.

**Upgrading:** Open the new configurator and click **Save changes** to update the deployed launcher.

**When Codex .env configuration is enabled:** Save your work first. After saving, the configurator tries to quit the entire ChatGPT desktop app. Follow the message at the bottom before reopening ChatGPT.

## ✨ What's new

- Desktop and startup shortcuts are now named **ChatGPT Launcher**. Saving settings migrates shortcuts created by this tool under the old name.
- The PowerShell headless entry point supports registering, configuring, disabling, removing and listing companion extensions for third-party integrations.
- Expanded the [developer guide](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.1.2/docs/developer-guide.md) and examples covering launcher discovery, deployment when needed, dependencies and failure handling. The developer guide is in Chinese.

## 🛠️ Fixes

- Improved rollback after interrupted deployments and memory allocation failures. Incomplete recovery is clearly reported.
- Shortcut contents are checked again before removal. Restoring a removed shortcut during rollback no longer overwrites a newly created file with the same name.
- Reduced temporary allocations in the launcher and configurator. Buffers and handles no longer needed during companion startup waits are released promptly.
- A theme preference save failure no longer hides the reminder to quit and reopen ChatGPT.

## ⚠️ Known issues

- **Silent startup is currently unavailable.** The option to hide the main window at launch is disabled. ChatGPT may still show its main window even if an older configuration retains this option.
- **You may still need to quit ChatGPT manually.** If the app does not respond, its window cannot be activated, or background processes remain, the configurator asks you to quit and reopen it manually. It does not forcibly terminate processes.

<!-- Release tag: v1.1.2. After running release.cmd, upload the four attachments in Releases/v1.1.2/. Copy this text into the Release body; do not upload it as an attachment. -->

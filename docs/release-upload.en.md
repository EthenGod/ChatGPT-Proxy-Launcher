[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.ja.md)

# v1.1.0 Background deployment and companion launch chains

[![Quick start: step-by-step instructions](https://img.shields.io/badge/First_time-Quick_start-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/README.en.md#quick-start) [![Download ChatGPTLauncherSetup.exe for Windows x64](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.0/ChatGPTLauncherSetup.exe)

First time here? Open **Quick start** and follow the steps to launch the configurator with one PowerShell command or download it manually. If the badges do not appear, use the [text guide](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/README.en.md#quick-start) or [download the configurator directly](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.0/ChatGPTLauncherSetup.exe). Install the ChatGPT Windows app first. This is not an official OpenAI product.

**Upgrading:** Open the new configurator and click **Save changes** to update the deployed launcher.

**When Codex .env configuration is enabled:** Save your work first. After saving, the configurator tries to quit the entire ChatGPT desktop app. Follow the message at the bottom before reopening ChatGPT.

## ✨ What's new

- The configurator and PowerShell entry point support background deployment with parameters, for use by scripts, plugins and other tools.
- A companion launch interface supports arguments, dependency order, readiness checks and failure notifications. Limited retries are available when the safety requirements are met. The existing **Add program…** feature still works.
- Added a [developer integration guide (Chinese)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.1.0/docs/developer-guide.md), configuration examples and interface specifications.

## 🛠️ Fixes

- Fixed the UI sometimes remaining in a processing state after saving or port detection finished.
- Improved handling of invalid companion settings. Damaged registrations can still be disabled or removed. If the launch policy cannot be read, an error is reported instead of starting companions with assumed defaults.
- Reduced repeated work during proxy detection, rendering and configuration parsing, and improved resource cleanup on errors.

## ⚠️ Known issues

- **Silent startup is currently unavailable.** The option to hide the main window at launch is disabled. ChatGPT may still show its main window even if an older configuration retains this option.
- **You may still need to quit ChatGPT manually.** If the app does not respond, its window cannot be activated, or background processes remain, the configurator asks you to quit and reopen it manually. It does not forcibly terminate processes.

<!-- Release tag: v1.1.0. After running release.cmd, upload the four attachments in Releases/v1.1.0/. Copy this text into the Release body; do not upload it as an attachment. -->

[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.ja.md)

> This document may include machine-translated content. If anything differs, refer to the [Chinese original](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.md).

# v1.2.1

[![Download the Windows x64 configurator](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe)

Double-click the download to open it. To update an existing installation, choose **Save changes**. [Direct download](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe) | [User guide](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/README.en.md)

## What's new

- The redesigned configurator puts startup options, proxy settings, and the argument preview on one page. Turning the proxy on checks the port once. Click the notification in the lower right to see where the port was found.
- Open **Advanced settings** to see which companion programs start with ChatGPT. Programs that support the launcher’s integration protocol have a badge. Select a name to see its path and arguments.
- PowerShell downloads stable releases by default. Use `-Channel Preview` to include prereleases, `-Branch` to filter by branch, or `-Tag` to choose a specific release.

## Fixes

- Opening the configurator again restores its minimized window. Clicking Save repeatedly writes the settings once.
- The configurator reads the deployed settings back after saving. If the write succeeds but the read fails, it still shows the settings as saved and asks you to reopen the configurator.

## Before saving

If WebView2 is missing, double-clicking the configurator opens compatibility mode. Opening it from PowerShell also gives you the option to install WebView2.

Silent startup remains unavailable. When applying proxy settings to Codex, save your work in ChatGPT before saving the settings. The configurator will try to quit ChatGPT.

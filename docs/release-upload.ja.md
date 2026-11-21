[简体中文](release-upload.md) | [English](release-upload.en.md) | [日本語](release-upload.ja.md)

> 機械翻訳を含む場合があります。内容が異なる場合は[中国語原版](release-upload.md)を参照してください。

# v1.2.5 保存後のメッセージが読みやすくなりました

[![クイックスタート：初めての方へ](https://img.shields.io/badge/%E5%88%9D%E3%82%81%E3%81%A6%E3%81%AE%E6%96%B9-%E3%82%AF%E3%82%A4%E3%83%83%E3%82%AF%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.5/README.ja.md#%E3%82%AF%E3%82%A4%E3%83%83%E3%82%AF%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88) [![Windows x64 設定ツールをダウンロード](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.5/ChatGPTLauncherSetup.exe)

**初めての方は[クイックスタート](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.5/README.ja.md#%E3%82%AF%E3%82%A4%E3%83%83%E3%82%AF%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88)をご覧ください。** PowerShell の 1 行コマンドは、既定で最新の正式版をダウンロードして設定ツールを開きます。ガイドには手動ダウンロードと初回設定の手順もあります。

**旧版から更新する場合**は、新しい設定ツールを開き、**変更を保存** を押してください。[v1.2.5 の設定ツールをダウンロード](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.5/ChatGPTLauncherSetup.exe)

**Codex の環境変数に適用** をオンにしている方は、先に ChatGPT と Codex での作業を保存してください。設定を保存すると、設定ツールが ChatGPT の終了を試みます。

## ✨ 更新内容

- 保存後のダイアログで、タイトルと本文の間隔を広げました。

## 🛠️ 修正内容

- 保存後のメッセージが複数行に折り返されても、メッセージと詳細ボタンが上下中央に揃います。
- 英語や日本語の長い設定名が折り返されても、隣のヘルプボタンが上下中央に揃います。

## ⚠️ 既知の問題

- サイレント起動は引き続き利用できません。起動時に ChatGPT のメインウィンドウが表示される場合があります。
- ChatGPT がまだ起動中と表示された場合は、手動で完全に終了し、**ChatGPT Launcher** ショートカットから起動してください。

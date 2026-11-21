[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.ja.md)

> 機械翻訳を含む場合があります。内容が異なる場合は[中国語原版](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.md)を参照してください。

# 🚀 v1.2.1 設定画面を一新

[![クイックスタート：初めての方へ](https://img.shields.io/badge/%E5%88%9D%E3%82%81%E3%81%A6%E3%81%AE%E6%96%B9-%E3%82%AF%E3%82%A4%E3%83%83%E3%82%AF%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/README.ja.md#%E3%82%AF%E3%82%A4%E3%83%83%E3%82%AF%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88) [![Windows x64 設定ツールをダウンロード](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe)

**初めての方は[クイックスタート](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/README.ja.md#%E3%82%AF%E3%82%A4%E3%83%83%E3%82%AF%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88)をご覧ください。** PowerShell の 1 行コマンドは、既定で最新の正式版をダウンロードして設定ツールを開きます。ガイドには手動ダウンロードと初回設定の手順もあります。

**旧版から更新する場合**は、新しい設定ツールを開き、**変更を保存** を押してください。[v1.2.1 の設定ツールをダウンロード](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe)

**Codex にも適用** をオンにしている方は、先に ChatGPT と Codex での作業を保存してください。設定を保存すると、設定ツールが ChatGPT の終了を試みます。

## ✨ 更新内容

- ログイン時の起動、デスクトップショートカット、プロキシを最初の画面で設定できます。変更すると、起動引数のプレビューに反映されます。
- プロキシをオンにすると、見つかったポート番号が自動で入力されます。どこから取得したかは右下の通知で確認できます。
- **詳細設定** には、ChatGPT と一緒に起動するプログラムの一覧があり、ここから新しく追加できます。連携に対応しているものや、無効にしたものには印が付きます。
- WebView2 が入っていないパソコンでも、設定ツールをダブルクリックすれば互換モードで設定を変更できます。
- PowerShell でダウンロードすると、通常は正式版が選ばれます。新機能を試したい方はテスト版も選べます。

## 🛠️ 修正内容

- 言語を切り替えたときに画面全体が一瞬黒くなる問題を修正しました。入力中のプロキシのアドレスとポート番号もそのまま残ります。
- 設定ツールを最小化しても、もう一度ダブルクリックすれば元の画面に戻って作業を続けられます。
- 設定を保存できたのに、失敗したと表示されることがある問題を修正しました。

## ⚠️ 既知の問題

- サイレント起動は引き続き利用できません。起動時に ChatGPT のメインウィンドウが表示される場合があります。
- ChatGPT がまだ起動中と表示された場合は、手動で完全に終了し、開き直してください。新しい Codex のプロキシ設定が反映されます。

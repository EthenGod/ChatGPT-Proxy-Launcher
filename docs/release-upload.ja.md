[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.ja.md)

> 機械翻訳を含む場合があります。内容が異なる場合は[中国語原版](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.md)を参照してください。

# v1.2.1

[![Windows x64 設定ツールをダウンロード](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe)

ダウンロードしたファイルをダブルクリックしてください。既存のランチャーは **変更を保存** で更新できます。[直接ダウンロード](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe) ｜ [使い方](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/README.ja.md)

## 更新内容

- 新しい設定画面では、起動オプション、プロキシ設定、起動引数のプレビューを同じページで確認できます。プロキシをオンにするとポートを一度検出します。右下の通知をクリックすると、検出元がわかります。
- **詳細設定** で、ChatGPT と一緒に起動するプログラムを確認できます。ランチャーとの連携に対応したプログラムには印が付きます。名前を選ぶとパスと起動引数が表示されます。
- PowerShell は既定で正式版をダウンロードします。`-Channel Preview` でテスト版を含め、`-Branch` でブランチを絞り込み、`-Tag` で特定のバージョンを指定できます。

## 修正内容

- 最小化中に設定ツールをもう一度開くと、元のウィンドウが表示されます。保存ボタンを続けて押しても、書き込みは一度だけです。
- 保存後に設定を読み直します。書き込みに成功し、読み直しに失敗した場合は、保存済みの表示を保ち、設定ツールを開き直すよう案内します。

## 保存前の注意

WebView2 がない環境で設定ツールをダブルクリックすると、互換モードで開きます。PowerShell から開いた場合は、WebView2 のインストールも選べます。

サイレント起動は引き続き利用できません。Codex にプロキシ設定を適用するときは、設定を保存する前に ChatGPT での作業を保存してください。設定ツールは ChatGPT の終了を試みます。

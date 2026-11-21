[简体中文](release-upload.md) | [English](release-upload.en.md) | [日本語](release-upload.ja.md)

> 機械翻訳を含む場合があります。内容が異なる場合は[中国語原版](release-upload.md)を参照してください。

# v1.2.4 ウィンドウの幅に合わせて文字が滑らかに折り返されます

[![クイックスタート：初めての方へ](https://img.shields.io/badge/%E5%88%9D%E3%82%81%E3%81%A6%E3%81%AE%E6%96%B9-%E3%82%AF%E3%82%A4%E3%83%83%E3%82%AF%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.4/README.ja.md#%E3%82%AF%E3%82%A4%E3%83%83%E3%82%AF%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88) [![Windows x64 設定ツールをダウンロード](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.4/ChatGPTLauncherSetup.exe)

**初めての方は[クイックスタート](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.4/README.ja.md#%E3%82%AF%E3%82%A4%E3%83%83%E3%82%AF%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88)をご覧ください。** PowerShell の 1 行コマンドは、既定で最新の正式版をダウンロードして設定ツールを開きます。ガイドには手動ダウンロードと初回設定の手順もあります。

**旧版から更新する場合**は、新しい設定ツールを開き、**変更を保存** を押してください。[v1.2.4 の設定ツールをダウンロード](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.4/ChatGPTLauncherSetup.exe)

**Codex の環境変数に適用** をオンにしている方は、先に ChatGPT と Codex での作業を保存してください。設定を保存すると、設定ツールが ChatGPT の終了を試みます。

## ✨ 更新内容

- 設定ツールの幅を変えると、設定項目、起動引数、ヘルプの文字が滑らかに折り返されます。幅を狭めるとプロキシの入力欄が並び替わり、必要に応じてスクロールバーが現れます。
- Windows のアニメーション効果をオフにしている場合、文字はすぐに折り返されます。

## 🛠️ 修正内容

- 下部のメッセージの行数が変わっても、「閉じる」と「変更を保存」は上下中央に揃います。保存前の案内が入力エラーに切り替わるときも、文字の位置が安定します。

## ⚠️ 既知の問題

- サイレント起動は引き続き利用できません。起動時に ChatGPT のメインウィンドウが表示される場合があります。
- ChatGPT がまだ起動中と表示された場合は、手動で完全に終了し、**ChatGPT Launcher** ショートカットから起動してください。

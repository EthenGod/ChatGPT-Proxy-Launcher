[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.ja.md)

> この文書には機械翻訳による内容が含まれる場合があります。内容に相違がある場合は、[中国語原版](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.md)を優先してください。

# v1.1.3 デスクトップショートカットの表示を修正

[![クイックスタート：手順を見る](https://img.shields.io/badge/First_time-Quick_start-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/README.ja.md#クイックスタート) [![ChatGPTLauncherSetup.exe をダウンロード（Windows x64）](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.3/ChatGPTLauncherSetup.exe)

初めて使う場合は **クイックスタート** を開き、PowerShell の 1 行コマンド、または手動ダウンロードの手順に従ってください。ボタンが表示されない場合は、[説明を読む](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/README.ja.md#クイックスタート)か、[設定ツールを直接ダウンロード](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.3/ChatGPTLauncherSetup.exe)できます。先に ChatGPT の Windows アプリをインストールしてください。本ツールは OpenAI の公式製品ではありません。

**旧バージョンから更新する場合：** 新しい設定ツールを開き、**変更を保存** をクリックすると、配置済みランチャーが更新されます。

**Codex の .env 設定を有効にする場合：** 先に作業を保存してください。保存後、設定ツールは ChatGPT デスクトップアプリ全体の終了を試みます。画面下部の案内に従って ChatGPT を開き直してください。

## ✨ 更新内容

- デスクトップのショートカットを作成・更新・削除した後、設定ツールが Windows にデスクトップ表示の更新を通知します。

## 🛠️ 修正内容

- デスクトップフォルダーにショートカットファイルが存在していても、F5 キーで更新するまでアイコンが表示されない問題を修正しました。
- 旧名称のショートカットを移行した後や、保存失敗時に元の状態へ戻した後も、デスクトップ表示を更新します。

## ⚠️ 既知の問題

- **サイレント起動は現在利用できません。** 起動時にメインウィンドウを表示しない設定は無効になっています。旧設定に選択状態が残っていても、ChatGPT のメインウィンドウが表示される場合があります。
- **ChatGPT の手動終了が必要な場合があります。** アプリが終了要求に応答しない、ウィンドウをアクティブにできない、バックグラウンドで動作が続く場合は、手動で終了して開き直すよう案内します。プロセスを強制終了しません。

<!-- リリースタグ：v1.1.3。release.cmd を実行後、Releases/v1.1.3/ 内の 4 ファイルを添付してください。この説明は Release 本文にコピーし、添付ファイルには含めません。 -->

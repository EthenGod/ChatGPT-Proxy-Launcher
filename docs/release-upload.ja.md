[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.ja.md)

# v1.1.0 バックグラウンドでの配置とプラグインの連携起動

[![クイックスタート：手順を見る](https://img.shields.io/badge/First_time-Quick_start-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/README.ja.md#クイックスタート) [![ChatGPTLauncherSetup.exe をダウンロード（Windows x64）](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.0/ChatGPTLauncherSetup.exe)

初めて使う場合は **クイックスタート** を開き、PowerShell の 1 行コマンド、または手動ダウンロードの手順に従ってください。ボタンが表示されない場合は、[説明を読む](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/README.ja.md#クイックスタート)か、[設定ツールを直接ダウンロード](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.0/ChatGPTLauncherSetup.exe)できます。先に ChatGPT の Windows アプリをインストールしてください。本ツールは OpenAI の公式製品ではありません。

**旧バージョンから更新する場合：** 新しい設定ツールを開き、**変更を保存** をクリックすると、配置済みランチャーが更新されます。

**Codex の .env 設定を有効にする場合：** 先に作業を保存してください。保存後、設定ツールは ChatGPT デスクトップアプリ全体の終了を試みます。画面下部の案内に従って ChatGPT を開き直してください。

## ✨ 更新内容

- 設定ツールと PowerShell から、引数を指定してバックグラウンドで配置できるようになりました。スクリプトやプラグインなどの外部ツールから利用できます。
- プラグインの連携起動インターフェースを追加しました。起動引数、依存順序、起動完了の確認、失敗通知に対応します。安全条件を満たす場合は、回数を制限して再試行できます。従来の **プログラムを追加…** も引き続き使えます。
- [開発者向け導入ガイド（中国語）](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.1.0/docs/developer-guide.md)、設定例、インターフェース仕様を追加しました。

## 🛠️ 修正内容

- 保存やポート検出が終わっても、画面が処理中のままになる場合がある問題を修正しました。
- プラグイン設定の異常時の処理を改善しました。破損した登録も無効化や削除ができます。起動ポリシーを読み取れない場合は、既定値で起動せず、エラーを表示します。
- プロキシ検出、画面描画、設定解析の重複処理を減らし、エラー時のリソース解放を改善しました。

## ⚠️ 既知の問題

- **サイレント起動は現在利用できません。** 起動時にメインウィンドウを表示しない設定は無効になっています。旧設定に選択状態が残っていても、ChatGPT のメインウィンドウが表示される場合があります。
- **ChatGPT の手動終了が必要な場合があります。** アプリが終了要求に応答しない、ウィンドウをアクティブにできない、バックグラウンドで動作が続く場合は、手動で終了して開き直すよう案内します。プロセスを強制終了しません。

<!-- リリースタグ：v1.1.0。release.cmd を実行後、Releases/v1.1.0/ 内の 4 ファイルを添付してください。この説明は Release 本文にコピーし、添付ファイルには含めません。 -->

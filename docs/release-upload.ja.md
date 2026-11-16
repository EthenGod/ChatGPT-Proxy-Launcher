[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.ja.md)

> この文書には機械翻訳による内容が含まれる場合があります。内容に相違がある場合は、[中国語原版](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/docs/release-upload.md)を優先してください。

# v1.1.2 安定性と追加プログラム連携の改善

[![クイックスタート：手順を見る](https://img.shields.io/badge/First_time-Quick_start-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/README.ja.md#クイックスタート) [![ChatGPTLauncherSetup.exe をダウンロード（Windows x64）](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.2/ChatGPTLauncherSetup.exe)

初めて使う場合は **クイックスタート** を開き、PowerShell の 1 行コマンド、または手動ダウンロードの手順に従ってください。ボタンが表示されない場合は、[説明を読む](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/master/README.ja.md#クイックスタート)か、[設定ツールを直接ダウンロード](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.1.2/ChatGPTLauncherSetup.exe)できます。先に ChatGPT の Windows アプリをインストールしてください。本ツールは OpenAI の公式製品ではありません。

**旧バージョンから更新する場合：** 新しい設定ツールを開き、**変更を保存** をクリックすると、配置済みランチャーが更新されます。

**Codex の .env 設定を有効にする場合：** 先に作業を保存してください。保存後、設定ツールは ChatGPT デスクトップアプリ全体の終了を試みます。画面下部の案内に従って ChatGPT を開き直してください。

## ✨ 更新内容

- デスクトップとスタートアップのショートカット名を **ChatGPT Launcher** に統一しました。設定を保存すると、本ツールが旧名称で作成したショートカットを移行します。
- PowerShell のバックグラウンド実行で、連携する追加プログラムの登録、設定、無効化、削除、一覧取得ができるようになりました。
- [開発者ガイド](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.1.2/docs/developer-guide.md)と例を拡充し、ランチャーの検出、必要に応じたバックグラウンド配置、依存関係、失敗時の処理を説明しました。開発者ガイドは中国語です。

## 🛠️ 修正内容

- 配置の中断やメモリ確保の失敗時に行う復元処理を改善しました。完全に元へ戻せない場合は明確に案内します。
- ショートカットを削除する直前に内容を再確認します。削除したショートカットを復元する際も、新たに作成された同名ファイルを上書きしません。
- ランチャーと設定ツールの一時的なメモリ確保を減らしました。追加プログラムの起動待ちで不要になったバッファとハンドルは早期に解放します。
- テーマの設定を保存できない場合も、ChatGPT を終了して開き直す案内を残します。

## ⚠️ 既知の問題

- **サイレント起動は現在利用できません。** 起動時にメインウィンドウを表示しない設定は無効になっています。旧設定に選択状態が残っていても、ChatGPT のメインウィンドウが表示される場合があります。
- **ChatGPT の手動終了が必要な場合があります。** アプリが終了要求に応答しない、ウィンドウをアクティブにできない、バックグラウンドで動作が続く場合は、手動で終了して開き直すよう案内します。プロセスを強制終了しません。

<!-- リリースタグ：v1.1.2。release.cmd を実行後、Releases/v1.1.2/ 内の 4 ファイルを添付してください。この説明は Release 本文にコピーし、添付ファイルには含めません。 -->

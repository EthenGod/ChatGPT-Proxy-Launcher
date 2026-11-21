[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/docs/release-upload.ja.md)

> 機械翻訳を含む場合があります。内容が異なる場合は[中国語原版](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/docs/release-upload.md)を参照してください。

# v1.2.2 クラウドの Codex 会話でのプロキシ利用と起動引数のプレビュー

[![クイックスタート：初めての方へ](https://img.shields.io/badge/%E5%88%9D%E3%82%81%E3%81%A6%E3%81%AE%E6%96%B9-%E3%82%AF%E3%82%A4%E3%83%83%E3%82%AF%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/README.ja.md#%E3%82%AF%E3%82%A4%E3%83%83%E3%82%AF%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88) [![Windows x64 設定ツールをダウンロード](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.2/ChatGPTLauncherSetup.exe)

**初めての方は[クイックスタート](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/README.ja.md#%E3%82%AF%E3%82%A4%E3%83%83%E3%82%AF%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88)をご覧ください。** PowerShell の 1 行コマンドは、既定で最新の正式版をダウンロードして設定ツールを開きます。ガイドには手動ダウンロードと初回設定の手順もあります。

**旧版から更新する場合**は、新しい設定ツールを開き、**変更を保存** を押してください。[v1.2.2 の設定ツールをダウンロード](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.2/ChatGPTLauncherSetup.exe)

**Codex の環境変数に適用** をオンにしている方は、先に ChatGPT と Codex での作業を保存してください。設定を保存すると、設定ツールが ChatGPT の終了を試みます。

## ✨ 更新内容

- **クラウドの Codex 会話に接続できない問題を修正** が標準で有効になりました。認証不要の HTTP または HTTPS プロキシを 1 つ設定したら、ChatGPT を完全に終了し、設定済みの **ChatGPT Launcher** ショートカットから起動してください。新しく起動したプロセスは、クラウドの会話にも選択したプロキシを使えます。プロキシの除外ルールを指定した場合は、既存のプロキシ環境を維持します。
- 通常の設定画面では、**起動引数のプレビュー** の内容が変わると文字がフェードで切り替わります。ウィンドウが狭くても左下のメッセージは行の途中で切れません。文字を選択しているときや、アニメーションを減らす設定が有効なときは、最新の引数をすぐに表示します。
- 追加プログラムの起動を待つ前に、読み込み済みの設定データを解放し、待機中のメモリ使用量を減らしました。

## 🛠️ 修正内容

- 既存の Codex `.env` ファイルを更新しても、元のアクセス権が保持されます。保存に失敗した場合は、復元に必要なファイルのコピーを保護します。
- 追加プログラムに必要な依存先が見つからない場合、設定画面に理由が表示されます。PowerShell から設定画面を開いて処理が失敗したときも、スクリプトがエラーを返します。

## ⚠️ 既知の問題

- サイレント起動は引き続き利用できません。起動時に ChatGPT のメインウィンドウが表示される場合があります。
- ChatGPT がまだ起動中と表示された場合は、手動で完全に終了し、**ChatGPT Launcher** ショートカットから起動してください。

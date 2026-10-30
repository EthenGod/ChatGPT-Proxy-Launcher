# v1.0.3 界面更新与桌面快捷方式

非官方 Windows x64 ChatGPT 启动器。使用前请先安装 ChatGPT Windows 应用。

[![下载 ChatGPTLauncherSetup.exe（Windows x64，推荐）](https://img.shields.io/badge/Windows%20x64-%E4%B8%8B%E8%BD%BD%20ChatGPTLauncherSetup.exe-2563eb?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.0.3/ChatGPTLauncherSetup.exe)

[按钮无法显示？直接下载 ChatGPTLauncherSetup.exe](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.0.3/ChatGPTLauncherSetup.exe)

**普通用户只需下载上面的部署器。** 它已包含启动器，无需额外安装 .NET 或 WebView2。

## 更新内容

- 新增“创建桌面快捷方式”选项，默认关闭。开启并保存后，可从桌面使用当前参数启动；以后修改参数或部署目录时，快捷方式会同步更新。
- 界面默认深色。右上角太阳／月亮按钮切换浅深色，并记住选择。
- 精简界面文字，统一分组、字号和间距，加入轻微的开关及悬停过渡。
- 优化代理协议的选中高亮，减少重叠边框和原生控件闪现，保留键盘操作。

## 使用提示

1. 打开部署器，按需要设置登录自启、桌面快捷方式和代理。
2. 点击“部署并保存”。若出现自启核对提示，按提示检查 ChatGPT 自带自启是否已关闭。
3. 完成后关闭部署器。保存不会立即启动 ChatGPT。

**旧版本用户：**运行新版部署器并保存，即可更新启动器和快捷方式。

**静默启动仍暂不可用，相关选项保持置灰。** 代理端口检测失败时，请到代理工具的端口设置中查看并手动填写。

## 附件说明

| 文件 | 用途 |
| --- | --- |
| **ChatGPTLauncherSetup.exe** | **推荐下载，图形化部署和配置** |
| ChatGPTProxyLauncher.exe | 手动配置快捷方式参数的独立启动器 |
| ChatGPTLauncher-source.zip | 本次版本的源码和构建脚本 |
| SHA256SUMS.txt | 下载文件校验值 |

GitHub 自动提供的 Source code 也是源码，不是安装程序。

<!--
维护者发布提示：
标签使用 v1.0.3，上传 Releases/v1.0.3/ 中的四个附件。
本文件可复制到 Release 正文，release.cmd 会同步到 Releases/README.md；该说明不作为附件上传。
下载链接需在对应 Release 发布并上传附件后才生效。打包不会自动发布到 GitHub。
-->

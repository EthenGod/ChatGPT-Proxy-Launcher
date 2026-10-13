# Release 上传说明

打开对应版本文件夹，将里面的 4 个文件上传到 GitHub Release：

- `ChatGPTLauncherSetup.exe`：推荐普通用户下载，图形化部署和配置，已内置启动器。
- `ChatGPTProxyLauncher.exe`：独立启动器，供手动配置快捷方式的用户使用。
- `ChatGPTLauncher-source.zip`：与本次编译对应的源码及构建脚本。
- `SHA256SUMS.txt`：上述文件的校验值。

本说明不需要上传。重新生成附件，在项目根目录运行 `release.cmd`。

## 可复制到 Release 页的简短说明

非官方 Windows x64 ChatGPT 启动器。支持图形化部署、登录自启、启动参数配置，以及常见代理工具的端口检测。

普通用户下载 `ChatGPTLauncherSetup.exe`，设置选项后点击“部署并保存”即可。无需额外安装 .NET 或 WebView2。

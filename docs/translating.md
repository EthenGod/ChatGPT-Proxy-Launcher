# 社区翻译指南

中文是基准。英语可作参考。界面、帮助、错误提示、启动器通知和 PowerShell 下载提示都在 `locales/`，无需修改 C++。翻译随程序发布，不需要用户另外下载语言包。

## 修改已有翻译

1. 找到对应文件：`zh-CN.json`、`en-US.json` 或 `ja-JP.json`。
2. 只改 `strings` 中右侧的文本。左侧的键用于代码查找，不改名。
3. 文件使用 UTF-8。换行写成 `\n`，段落间写成 `\n\n`。双引号写成 `\"`，反斜杠写成 `\\`。
4. 保留 `{path}`、`{version}` 等占位符的名称。可以调整位置，不要翻译或删除。变量的内容会原样填入，不会当作另一条翻译处理。
5. 在项目根目录运行下面的生成和检查命令。提交语言文件，以及自动更新的 `setup.ps1`。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/locales.ps1 -Generate
powershell -NoProfile -ExecutionPolicy Bypass -File tools/locales.ps1
```

校验不需要 Visual Studio。`-Generate` 更新内置文本；不带参数只检查，不修改文件。不要手改 `work/LocalizationData.h` 或 `setup.ps1` 的 `BEGIN GENERATED LOCALES` 区域。

## 添加新语言

1. 复制 `locales/zh-CN.json`，例如命名为 `fr-FR.json`。
2. 将 `locale` 改为 `fr-FR`，`nativeName` 改为该语言的本地名称，例如 `Français`。菜单直接使用这个名称，请简短。
3. `fontFamily` 填 Windows 自带且适合该语言的字体。拉丁文字可以用 `Segoe UI`。字体名称最多 31 个字符。首批只支持从左到右的布局。
4. 在 `locales/languages.json` 的 `languages` 数组末尾加入 `{"tag":"fr-FR","complete":false}`。保留中文在首位。文件名与 `locale`、`tag` 必须一致。
5. 翻译 `strings`。未完成的键可以删除，或把文本留空。程序会使用对应中文。全部完成后把 `complete` 改为 `true`，漏翻会阻止构建。
6. 运行上面的命令。语言会自动进入菜单、两个 EXE、PowerShell 文案和源码发布包，不需要修改 C++ 或发布清单。

三种正式内置语言必须完整。新增语言可以先提交部分翻译。校验会报告缺少多少条。编辑器可使用 `locales/catalog.schema.json` 提供格式提示；项目脚本还会检查重复键、未知键、占位符、字符和大小限制。

## 文案约定

- 界面用短句。按钮说明操作，错误说明出了什么问题以及下一步怎么做。
- `setup.*` 用于配置器；`help.*` 用于问号帮助；`error.*` 用于部署或输入错误；`proxy.*` 用于端口检测；`startup.*` 用于自启动报告；`chain.*` 用于附加程序通知；`download.*` 用于 PowerShell。
- 技术名称保留原样：ChatGPT、Codex、`.env`、HTTP、SOCKS5、PowerShell 等。
- 不翻译 `--proxy-server` 等参数、JSON 字段、机器错误代码、文件名、环境变量名和管理标记。程序和脚本依赖这些固定值。
- 整句带变量时用命名占位符。不要添加靠前后拼接才能读懂的半句话。翻译不使用 HTML 或 Markdown 格式；问号帮助中的换行会直接显示。
- 系统对话框按钮、系统错误详情可能仍使用 Windows 提供的语言。第三方程序自己的名称和错误不由本项目翻译。

## 检查实际显示

安装 Visual Studio C++ 桌面开发组件后，在根目录运行：

```powershell
cmd /c tests\test-build.cmd
cmd /c tests\analyze.cmd
```

再打开 `outputs/ChatGPTLauncherSetup.exe`，从右上角选择新语言。检查主页面、高级页面、问号帮助和浅深色。缩小窗口，并检查 100%、150%、200% 显示缩放。按钮、开关和问号不能重叠，重要提示不能被截断。

自动测试只用隔离目录和替身，不部署到真实自启目录，不关闭 ChatGPT。截图留在 `work/`，不要提交带个人路径的截图。

新增功能时先更新中文基准，再更新英语、日语和调用代码。已有键的含义不变时保留键名；含义变化时检查所有译文。删掉不用的键时同步所有语言文件。`schemaVersion` 只在语言文件结构发生不兼容变化时升级。

## For community translators

Chinese (`zh-CN.json`) is the source and fallback language. Use `en-US.json` as an additional reference. Edit string values only; keep keys and named placeholders unchanged. Register a new locale in `languages.json`; no C++ edits are needed. Missing or empty entries fall back to Chinese when `complete` is `false`. Run the two commands above and include the regenerated `setup.ps1` in your pull request. Please check the actual UI for clipping and report which screens you reviewed.

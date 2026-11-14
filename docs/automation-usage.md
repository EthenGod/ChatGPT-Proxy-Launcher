# 自动化接入

适用于本地开发版 **v1.0.7** 及后续兼容版本。本次没有发布 Release。在线下载命令需要新版脚本及 EXE 已经发布；旧 EXE 不支持这些接口。

## 先查看计划，再部署

1. 从源码构建，或取得支持此功能的 `ChatGPTLauncherSetup.exe`。
2. 复制 [deploy-v1.json](examples/deploy-v1.json)，按需要填写路径、代理和选项。示例会开启自启、桌面图标及 `.env` 更新，请先检查内容。
3. 使用 `plan` 查看最终配置。它只创建结果文件，不部署。
4. 确认配置后，将操作改为 `deploy` 再运行。每次使用新的结果文件名。

在项目根目录的 PowerShell 中，本地预览示例为：

```powershell
$setup = (Resolve-Path '.\outputs\ChatGPTLauncherSetup.exe').Path
$request = (Resolve-Path '.\docs\examples\deploy-v1.json').Path
$result = Join-Path $env:TEMP ('cgl-plan-' + [guid]::NewGuid().ToString('N') + '.json')
$arguments = @('--headless', 'plan', '--request', ('"' + $request + '"'), '--result', ('"' + $result + '"'))
$process = Start-Process -FilePath $setup -ArgumentList $arguments -WindowStyle Hidden -PassThru
try { $process.WaitForExit(); $exitCode = $process.ExitCode } finally { $process.Dispose() }
$report = Get-Content -LiteralPath $result -Raw -Encoding UTF8 | ConvertFrom-Json
$report
```

`deploy` 不打开配置器、不启动 ChatGPT、不强制退出应用。写入 `.env` 后，结果中的 `actionsRequired` 会提醒重新打开 ChatGPT。关闭自带自启未能确认时，也只返回待办，不打开 Windows 设置。

没有旧配置时，自启、桌面图标和代理默认关闭。已有配置时，缺省项保留。`.env` 和自带自启属于本次动作，必须显式请求。

`plan.data.changes` 表示拟执行的动作。`save` 允许跳过相同内容；`remove-if-managed` 只移除本工具拥有的文件。计划不试写目录，不能保证部署时权限和磁盘状态仍相同。计划包含完整启动参数，请勿当作普通日志分享。

## PowerShell 下载后直接部署

新版发布后，可使用带参数的脚本块。不要用不带参数的 `irm ... | iex` 来做后台部署。

```powershell
$url = 'https://raw.githubusercontent.com/EthenGod/ChatGPT-Proxy-Launcher/master/setup.ps1'
$resultPath = Join-Path $env:TEMP ('cgl-deploy-' + [guid]::NewGuid().ToString('N') + '.json')
$report = & ([scriptblock]::Create((irm $url))) -Headless -Request 'D:\Automation\request.json' -Result $resultPath
$report
```

本地脚本使用相同参数：`& .\setup.ps1 -Headless -Request <绝对路径> -Result <新 JSON 路径>`。脚本仍从 GitHub 下载，不能用它测试尚未发布的本地 EXE。预检可加 `-Operation plan`；查询用 `-Operation info`，此时不传 `-Request`。

脚本检查下载文件的校验值和最低版本 **1.0.7**。不满足时返回 71，不运行旧版配置器。结束后清理本次下载文件，保留请求、结果和调用者会话。不显示进度、Star 提示或网页。

## 如何判断结果

先等待进程退出，再读取 JSON。结果文件须是新 `.json` 文件，父目录已存在。不能复用上次的结果路径；旧文件不会被覆盖。

| 退出码 | 调用方处理 |
| --- | --- |
| 0 | 操作完成；查询仍需看 `data.state` |
| 10 | 操作完成，检查 `warnings` 和 `actionsRequired` |
| 20 / 21 | 修正请求或更新支持的接口版本 |
| 30 / 31 | 处理所有权冲突，或等待另一个操作结束 |
| 40 | 提交失败，文件已回滚 |
| 41 | 状态不完整，先检查文件，不能直接重复部署 |
| 50 | 读取或预检失败，未确认提交 |
| 70 / 72 | 结果无法取得或写出，先查询实际状态 |
| 71 | 下载、校验或版本检查失败，尚未运行配置器 |

`committed=true` 表示文件已提交；`false` 表示未提交或已完整回滚；`null` 表示无法确认。不要只检查文件存在，也不要按中文提示匹配错误。独立自动化脚本可自行执行 `exit $report.exitCode`；下载脚本不替调用者退出。

完整字段与参数见[后台部署接口](automation-design.md)。

## 让插件按规范启动

1. 扩展作者在程序目录放置 `companion.json`。从[清单示例](examples/companion-v1.json)开始，填写稳定 ID、版本、入口和参数。
2. 用配置器命令注册。注册只保存信息，不执行插件：

```text
ChatGPTLauncherSetup.exe --headless companion register --manifest D:\Apps\ExampleAddon\companion.json --result D:\Automation\register-1.json
```

3. 用户参数独立保存。使用 `companion configure --request <文件> --result <新文件>`，请求例如：

```json
{"schemaVersion":1,"id":"org.example.assistant-addon","overrides":{"arguments":["--profile","default"],"maxAttempts":1}}
```

4. 作者升级程序时保留清单路径与 ID。用户覆盖项继续有效。迁移到新路径时注册命令加 `--replace`，它不会删除旧程序。
5. 移除注册使用 `companion remove --id <ID> --result <新文件>`。只删除这个 ID 的注册记录，程序和用户数据保留。

同一入口不要同时放在旧 `StartupApps` 目录。明确识别到重复入口时会报告冲突。没有实现回执的普通插件继续使用配置器的“添加程序”。

## 插件报告就绪

支持 EXE、PowerShell、VBS/JS/WSF、Python、Node.js、Java 和 BAT/CMD。脚本宿主必须已安装。BAT/CMD 的参数放在上下文，不能拼入 cmd 命令。`.lnk` 继续使用旧启动目录。

PowerShell 作者可将 [companion-context.ps1](examples/companion-context.ps1) 随插件分发，并在入口中点调用。先初始化实际功能，再报告就绪：

```powershell
. (Join-Path $PSScriptRoot 'companion-context.ps1')
$context = Read-CglContext
# 从 $context.input.arguments 和 $context.input.config 读取用户配置。
# 在此完成插件初始化。成功后才发送 ready，并保持主程序运行。
Send-CglReply -Context $context -Status ready
# 在此进入插件自己的主循环。不要发送 ready 后立即退出。
```

初始化失败时，调用 `Send-CglReply -Context $context -Status failed -ErrorCode invalid-config`，随后正常退出。只有已确认可以安全重试的临时错误才加 `-Retryable`。交接给已有实例时使用 `-Status already-running -ExistingProcessId <PID>`。

短任务使用 `exit-code` 模式，不写回执。长时间运行的插件使用 `signal`。就绪检查只覆盖初始化和随后 1 秒，不能证明长期健康。

重试默认关闭。启用后还必须确认上一轮整个进程族已经退出；进程残留、跟踪失败或配置错误均不重试。全链等待默认 60 秒，结束后启动器退出，插件可继续运行。

插件通过上下文得到启动器路径、版本、部署状态、本次尝试、参数及配置。独立程序通过 `discovery.json` 发现已部署启动器，再用受支持的 `--launcher-info` 核对。不要把新参数发送给旧版 EXE 来探测能力。

字段、依赖、全链预算、显式结果和兼容规则见[链式启动规范 v1](companion-protocol-v1.md)。

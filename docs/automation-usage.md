# 自动化用法

先读[开发者接入指南](developer-guide.md)。本页提供“查询 → 按需部署 → 注册”的完整命令，以及配置、停用、移除和结果处理。

原生 EXE 至少为 v1.1.0。本次新增的四个 PowerShell 扩展管理操作需要更新后的 `setup.ps1`；线上地址只有在脚本发布后才具备这些参数。本地 `setup.ps1` 也会从 GitHub 下载 EXE，不会使用 `outputs/` 中的构建。

## 选择下载版本

图形和后台命令共用三个可选参数。

| 参数 | 用法 |
|---|---|
| `-Channel Stable` | 默认。仅选择正式版。 |
| `-Channel Preview` | 包含正式版和测试版。按发布时间选择最新一个。 |
| `-Branch <名称>` | 仅接受指定分支提交历史中的已发布标签。分支从正式版切出时，共同祖先上的正式版也可能入选。 |
| `-Tag <标签>` | 固定一个已发布版本。仍检查通道和分支。下载测试版须同时传 `-Channel Preview`。 |

例如，在项目根目录用 v1.2.2 的配置器查询部署状态。结果路径须是新文件。

```powershell
$result = Join-Path $env:TEMP ('cgl-' + [guid]::NewGuid() + '.json')
& .\setup.ps1 -Headless -Operation info -Result $result -Branch master -Tag v1.2.2
```

下载器只使用已发布 Release 的附件。它先固定标签，再从同一标签下载校验清单和配置器。后台结果的 `transport.releaseTag` 给出选中的实际标签。

## 通过 PowerShell 完成首次接入

以下代码块按顺序使用。Windows PowerShell 5.1、PowerShell 7 均可。由你的安装流程先取得用户授权，后台接口不另外弹窗。

### 准备脚本和输入文件

在项目根目录使用本地新脚本：

```powershell
$ErrorActionPreference = 'Stop'
$bootstrap = [scriptblock]::Create(
    [IO.File]::ReadAllText((Resolve-Path '.\setup.ps1').Path))
$request = (Resolve-Path '.\docs\examples\deploy-minimal-v1.json').Path
$manifest = (Resolve-Path 'D:\Apps\ExampleAddon\companion.json').Path
```

将示例扩展路径换成你的实际清单。正式集成可改用已审核的线上入口：

```powershell
$url = 'https://raw.githubusercontent.com/EthenGod/ChatGPT-Proxy-Launcher/master/setup.ps1'
$bootstrap = [scriptblock]::Create((Invoke-RestMethod -Uri $url -TimeoutSec 30))
```

不要使用无参数的 `irm ... | iex`；它会进入图形安装流程。可以随安装器保存经审核的脚本，更新时再替换。脚本本身也属于可执行代码，不能只校验下载的 EXE 而忽略脚本来源。

下面的包装函数为每次操作生成新的结果文件，不退出调用者会话。结果保留在调用者选择的临时位置：

```powershell
function Invoke-CglOnline {
    param([hashtable]$Options)
    $resultPath = Join-Path $env:TEMP ('cgl-' + [guid]::NewGuid().ToString('N') + '.json')
    $report = & $bootstrap -Headless @Options -Result $resultPath
    if ($report.schemaVersion -ne 1 -or $report.exitCode -notin @(0, 10)) {
        throw "CGL: $($report.code), exit=$($report.exitCode), result=$resultPath"
    }
    return $report
}
```

这是示例错误出口。产品中应按[结果与恢复](#结果与恢复)分支处理，并展示必要的待办。不要循环重试写入。

### 查询状态

```powershell
$info = Invoke-CglOnline @{Operation='info'}
$state = $info.data.state
if ($state -notin @('managed', 'not-deployed')) {
    throw "CGL requires inspection or an approved repair: $state"
}
```

`info` 不需要 `Request`。退出码为 0 只表示查询完成，必须再看 `data.state`。旧版、文件丢失、记录无效和读取失败的处理见[状态表](companion-protocol-v1.md#查询结果形状)。

### 未部署时先计划，再部署

仅在 `not-deployed` 分支运行预检；`managed` 直接跳到能力检查：

```powershell
if ($state -eq 'not-deployed') {
    $plan = Invoke-CglOnline @{Operation='plan'; Request=$request}
    $plan.data
}
```

向用户展示计划中的部署目录和改动。最小请求保留已有设置，首次部署不启用代理、自启或桌面图标。用户认可最终请求后，由上层安装流程设置授权状态，再运行：

```powershell
$deploymentApproved = $false # 上层确认用户已同意该计划后才改为 $true。
if ($state -eq 'not-deployed') {
    if (!$deploymentApproved) { throw 'Deployment requires approval of the displayed plan.' }
    $deployed = Invoke-CglOnline @{Operation='deploy'; Request=$request}
    if ($deployed.committed -ne $true) { throw 'Deployment was not confirmed.' }
    $deployed.warnings
    $deployed.actionsRequired
    $info = Invoke-CglOnline @{Operation='info'}
}
```

安装器已在自己的界面取得授权时，直接传递该授权状态，不需要再弹一遍确认。修复 `legacy` 或 `missing` 也可使用相同请求与命令，但应作为明确的升级或修复动作；`invalid`、`unknown` 不自动覆盖。

部署不会启动或退出 ChatGPT。写入 `.env` 后返回 `restart-chatgpt`；自带自启无法确认时返回 `check-official-startup`。调用方负责传达这些待办。

### 检查能力，再注册

清单应已准备好；从[完整示例](examples/companion-v1.json)复制后替换 ID、入口和版本。根据清单核对能力：

```powershell
if ($info.data.state -ne 'managed') { throw 'No confirmed managed deployment.' }
$discovery = $info.data.discovery
$author = Get-Content -LiteralPath $manifest -Raw -Encoding UTF8 | ConvertFrom-Json
if ($discovery.schemaVersion -ne 1 -or
    $discovery.setupApiVersions -notcontains 1 -or
    $discovery.chainProtocolVersions -notcontains 1) {
    throw 'Unsupported CGL API or chain protocol.'
}
foreach ($required in @('companion-registry-v1') + @($author.requires)) {
    if ($discovery.capabilities -cnotcontains $required) {
        throw "CGL capability missing: $required"
    }
}
$registered = Invoke-CglOnline @{Operation='companion-register'; Manifest=$manifest}
$listed = Invoke-CglOnline @{Operation='companion-list'}
$item = @($listed.data.entries | Where-Object { $_.id -ceq $author.id })
if ($item.Count -ne 1 -or !$item[0].valid -or !$item[0].enabled) {
    throw 'Registration is missing, invalid or disabled.'
}
$registered.warnings
```

重复注册会保留覆盖项，包括用户的停用选择。若列表显示停用，不要擅自启用。注册只保存信息，不启动插件；下一次通过启动器打开 ChatGPT 才会触发它。

每次调用下载入口都会下载并校验一次配置器。适合首次接入或低频操作。需要连续调用、离线安装或频繁查询时，保存一份已核对的配置器并使用下面的本地入口，避免反复联网。

## 本地配置器命令

先从同一个受信任 Release 取得 `ChatGPTLauncherSetup.exe` 和 `SHA256SUMS.txt`，核对 SHA-256，或使用自己的源码构建。然后读取文件版本，**不要运行旧版 EXE 来试探接口**：

```powershell
$setup = (Resolve-Path '.\outputs\ChatGPTLauncherSetup.exe').Path
$v = [Diagnostics.FileVersionInfo]::GetVersionInfo($setup)
$version = [version]::new($v.FileMajorPart, $v.FileMinorPart, $v.FileBuildPart, $v.FilePrivatePart)
if ($version -lt [version]'1.1.0') { throw 'CGL Setup 1.1.0 or later is required.' }

function Invoke-CglLocal {
    param([string[]]$Arguments)
    $resultPath = Join-Path $env:TEMP ('cgl-' + [guid]::NewGuid().ToString('N') + '.json')
    $native = @('--headless') + $Arguments + @('--result', ('"' + $resultPath + '"'))
    $process = Start-Process -FilePath $setup -ArgumentList $native -WindowStyle Hidden -PassThru
    try { $process.WaitForExit(); $exitCode = $process.ExitCode } finally { $process.Dispose() }
    $report = Get-Content -LiteralPath $resultPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($report.schemaVersion -ne 1 -or $report.exitCode -ne $exitCode -or $exitCode -notin @(0, 10)) {
        throw "CGL: exit=$exitCode, result=$resultPath"
    }
    return $report
}
$info = Invoke-CglLocal @('info')
```

`Start-Process -ArgumentList` 会拼接字符串。下例给文件路径明确加双引号；只传受校验的本机文件路径，不在命令串中拼接插件的自由文本参数。其他语言优先使用进程 API 的参数列表，关闭 Shell 转发。

本地命令与下载参数一一对应：

| 操作 | 本地 `--headless` 后的参数 | 下载脚本参数 |
| --- | --- | --- |
| 查询 | `info` | `-Operation info` |
| 计划 | `plan --request <JSON>` | `-Operation plan -Request <JSON>` |
| 部署 | `deploy --request <JSON>` | `-Operation deploy -Request <JSON>` |
| 注册 | `companion register --manifest <JSON>` | `-Operation companion-register -Manifest <JSON>` |
| 迁移注册路径 | 上一行加 `--replace` | 上一行加 `-Replace` |
| 配置或停用 | `companion configure --request <JSON>` | `-Operation companion-configure -Request <JSON>` |
| 移除 | `companion remove --id <ID>` | `-Operation companion-remove -Id <ID>` |
| 列表 | `companion list` | `-Operation companion-list` |

所有本地命令还需要 `--result <新 JSON 文件>`；下载脚本需要 `-Headless -Result <新 JSON 文件>`。上述两个包装函数已生成此参数。

例如，只预检和注册已有部署上的扩展：

```powershell
$plan = Invoke-CglLocal @('plan', '--request', ('"' + $request + '"'))
$registered = Invoke-CglLocal @('companion', 'register', '--manifest', ('"' + $manifest + '"'))
$listed = Invoke-CglLocal @('companion', 'list')
```

调用原生接口的进程须先等待配置器退出，再读结果。不能以 EXE 被创建或结果文件已出现判断完成。

## 配置、停用和移除

配置请求例子如下。ID 应与清单一致：

```json
{
  "schemaVersion": 1,
  "id": "org.example.assistant-addon",
  "overrides": {
    "enabled": true,
    "arguments": ["--profile", "default"],
    "config": {"exampleOption": true},
    "maxAttempts": 1
  }
}
```

将它保存为 UTF-8 JSON 后执行：

```powershell
$configRequest = (Resolve-Path 'D:\Automation\addon-config.json').Path
$configured = Invoke-CglOnline @{Operation='companion-configure'; Request=$configRequest}
```

整个 `overrides` 都会替换。若只提交 `{"enabled":false}`，旧参数及配置覆盖项也会清除；需要保留时由调用方保存并提交完整对象。`list` 不回显私有参数，不能靠它重建用户配置。停用不关闭已运行的实例。

下面是独立的维护命令，不属于首次接入流程。原目录升级保留 ID 和清单路径即可；迁移路径使用新的清单路径并明确传入 `Replace`：

```powershell
$migrated = Invoke-CglOnline @{Operation='companion-register'; Manifest=$manifest; Replace=$true}
```

卸载扩展时移除指定 ID 的注册：

```powershell
$removed = Invoke-CglOnline @{Operation='companion-remove'; Id='org.example.assistant-addon'}
```

移除不删除扩展程序、启动器或用户数据，也不结束运行中的进程。

## 结果与恢复

脚本返回一个对象；原生 EXE 返回退出码并写 JSON。先读 `status`、`exitCode`、`code`，再按操作读 `data`；退出码及字段定义统一见[结果接口](automation-design.md#3-统一结果)，不要匹配翻译后的消息。

- 0 或 10 表示操作完成；10 仍需展示 `warnings` 和 `actionsRequired`。`info` 必须再判断 `data.state`。
- `committed:false` 可表示只读、无变更或完整回滚，不能单独判失败。注册未产生新写入也可成功。
- 20、21、30 先修正请求、兼容性或冲突；31 可有限退避重试，每次使用新结果路径。40 排除原因后重新计划；41、70、72 或 `committed:null` 先查询实际状态，不循环部署。
- 结果须为新 `.json` 文件，父目录已存在，不覆盖请求或清单。脚本结果还须位于固定管理目录之外。参数绑定错误可直接抛异常，输入校验失败也可能只有返回对象。
- 下载入口清理自己的临时文件，保留输入、最终结果和调用者会话。需要退出码时，由最外层执行 `exit $report.exitCode`。调用者按需要清理自己创建的结果文件。

`plan` 含完整最终参数，不能当作普通日志分享。成功部署不等于 ChatGPT 已运行或登录自启已验证。

## 验证真正的链式启动

注册和 `info` 都不运行插件。下面的命令会真实启动 ChatGPT，再等待扩展就绪；只在用户主动测试时执行，并先确认 `chain-run-result-v1`：

```text
ChatGPTProxyLauncher.exe --launcher-chain --result "D:\Automation\chain-new.json" --feedback quiet -- <ChatGPT 启动参数>
```

占位部分替换为实际参数，没有参数也保留 `--`。此入口不自动加载管理快捷方式中的参数。等待启动器退出后读取结果，检查 `chatgpt.processCreated` 和自己的 `entries` 状态。不要只看总退出码，也不要把 `started-unverified` 当作 `ready`。

正常启动无需显式结果文件。使用已配置的快捷方式即可。回执实现、短任务、已有实例和有限重试见[链式启动规范](companion-protocol-v1.md)；PowerShell 回执辅助函数见 [companion-context.ps1](examples/companion-context.ps1)。

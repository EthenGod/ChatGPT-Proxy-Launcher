# 链式启动规范 v1

状态：随 v1.1.0 提供，协议版本为 1。文档修订日期：2026-11-16。本次补全接入约定，不改变 v1 字段或运行语义。后台接口见[部署设计](automation-design.md)，可复制命令见[自动化用法](automation-usage.md)。

初次接入先看[开发者指南](developer-guide.md)。结构文件包括[作者清单](schemas/companion-v1.schema.json)、[配置请求](schemas/companion-config-v1.schema.json)、[回执](schemas/companion-reply-v1.schema.json)、[全链策略](schemas/companion-policy-v1.schema.json)。

本规范用于本机、当前用户主动注册的程序和脚本。它定义启动、参数、就绪确认、有限重试及发现接口。它不负责下载扩展、自动更新扩展、权限提升或持续监控。

安装动作不要放进每次运行的就绪流程。环境变量缺失不代表未安装；发现接口和缺失部署的处理见第 7 节。

## 1. 启动器保证什么

先创建 ChatGPT 进程，再启动扩展。扩展失败不撤销 ChatGPT 启动，也不强制结束任何应用。

必须区分三个状态：

| 状态 | 能证明什么 |
| --- | --- |
| `started-unverified` | 系统接受了启动请求，不能证明程序可用 |
| `ready` | 扩展报告就绪，且关联进程通过短暂存活检查 |
| 长期健康 | v1 不提供；启动器退出后不再监控 |

ChatGPT 进程创建成功也不代表其界面、登录或网络已经就绪。需要连接 ChatGPT 的扩展自行等待连接，再报告 `ready`。

旧 `StartupApps` 目录保持兼容。它只处理第一层现有文件，结果属于 `started-unverified`。不自动添加重试或依赖关系。

## 2. 清单与用户配置分开

扩展作者在扩展目录提供 `companion.json`。用户注册记录放在固定配置根目录：

```text
%LOCALAPPDATA%\ChatGPTProxyLauncher\
  discovery.json
  StartupApps\                         原有普通附加程序
  CompanionRegistry\
    org.example.assistant-addon.json   本工具管理的注册记录
  companion-policy.json               用户的全链设置
```

上述位置不随启动器 EXE 的自定义部署目录改变。普通启动没有注册项时不创建目录或协议文件，不做进程族跟踪。两个启动目录均不存在时直接返回；已建空目录仍检查全链设置。结果只在调用者明确要求时写入文件。

### 作者清单

完整样例见 [companion-v1.json](examples/companion-v1.json)，结构见 [Schema](schemas/companion-v1.schema.json)。

```json
{
  "schemaVersion": 1,
  "id": "org.example.assistant-addon",
  "name": "示例扩展",
  "version": "1.2.0",
  "requires": ["ready-file-v1"],
  "entry": {
    "runtime": "exe",
    "path": "Addon.exe",
    "workingDirectory": ".",
    "arguments": ["--profile", "default"]
  },
  "dependsOn": [],
  "readiness": {"mode": "signal", "timeoutMs": 15000},
  "retry": {"maxAttempts": 1, "delayMs": 1000, "safe": false},
  "config": {}
}
```

- `id` 是稳定身份。使用小写反向域名形式，不含版本号。改显示名称、路径或版本时保留 ID。作为文件名时还须通过 Windows 保留设备名检查。
- `version` 使用三段非负整数，各段范围为 0–2147483647，按数值比较。仅用于展示和最低版本比较。v1 不引入版本范围表达式或自动更新解析器。
- `requires` 列出必须支持的能力。不认识的能力必须拒绝，不按普通程序降级启动。
- `entry.path` 和 `workingDirectory` 相对清单所在目录解析。要求位于该目录内，拒绝绝对路径、`..`、UNC 和重解析点。运行宿主例外，见下一节。
- `dependsOn` 是 `{ "id": "...", "minimumVersion": "1.0.0" }` 的数组。省略 `minimumVersion` 表示不限制版本。不得重复、依赖自身或形成环。
- `config` 是扩展自己的 JSON 对象。启动器只传递，不解释。扩展在报告就绪前校验；配置错误不能重试。
- 清单和注册记录各最多 64 KiB，JSON 最多嵌套 16 层。允许开头的 UTF-8 BOM，输出不带 BOM。全局未知字段、重复 JSON 键、非法 UTF-8 和不支持的版本均报错。`config` 内允许扩展自己的字段。

### 用户覆盖项

注册记录由配置器写入，结构如下：

```json
{
  "schemaVersion": 1,
  "managedBy": "ChatGPTProxyLauncher",
  "id": "org.example.assistant-addon",
  "manifestPath": "D:\\Apps\\ExampleAddon\\companion.json",
  "overrides": {
    "enabled": true,
    "arguments": ["--profile", "my-profile"],
    "config": {"exampleOption": true},
    "timeoutMs": 15000,
    "maxAttempts": 1
  }
}
```

`enabled` 默认 true。其余覆盖项缺失时使用清单值。`arguments` 和 `config` 整体替换，不做隐式拼接或深层合并。清空参数使用空数组。用户不能通过覆盖项改变 ID、入口、依赖、就绪模式或 `safe` 声明。

`managedBy` 是格式归属标记，不是数字签名。缺失、值不同、ID 与文件名不同或记录无法安全解析时，不能替换或删除。记录顶层只允许示例中的五个字段；覆盖项只允许示例中的五个字段，禁止未知字段。配置命令的请求顶层只允许 `schemaVersion`、`id`、`overrides`。

有效超时范围为 1000–30000 ms。最多尝试 1–3 次，默认 1 次；多次尝试必须同时满足 `retry.safe=true` 和 `tracked-retry-v1`。只要声明 `safe:true` 就必须声明该能力，即使当前只尝试一次。参数上限和路径检查仍作用于覆盖后的结果。

作者更新清单不覆盖用户记录。用户只需保持清单路径稳定。更换安装目录时，通过注册命令显式更新清单路径并保留覆盖项。

### 注册接口

以下命令使用后台部署的结果结构和本用户写锁。结果路径必须是新 `.json` 文件。无界面命令不执行扩展。

```text
ChatGPTLauncherSetup.exe --headless companion register --manifest <绝对路径> --result <新文件>
ChatGPTLauncherSetup.exe --headless companion register --manifest <绝对路径> --replace --result <新文件>
ChatGPTLauncherSetup.exe --headless companion configure --request <绝对路径> --result <新文件>
ChatGPTLauncherSetup.exe --headless companion remove --id <ID> --result <新文件>
ChatGPTLauncherSetup.exe --headless companion list --result <新文件>
```

- `register`：校验清单并原子写入注册记录。同 ID、同路径重复注册可安全执行，保留用户覆盖项。同 ID、不同路径返回所有权冲突；只有显式 `--replace` 才可更新本工具已有的记录。
- `configure`：请求格式为 `{ "schemaVersion":1, "id":"...", "overrides":{...} }`，整体替换用户覆盖项。空对象恢复清单默认值。不能修改未注册项。`enabled:false` 验证请求及注册归属后可直接停用，不依赖作者清单、入口或其他扩展的依赖图；重新启用须完整校验。
- `remove`：只删除一个指定 ID 的、本工具管理的注册文件。不删除程序、清单或用户数据。没有该项也返回成功。
- `list`：只读返回每个 ID 的启用状态、版本、有效性和注册来源。默认不回显参数及私有配置。停用项不读取作者清单，版本为空；此时 `valid:true` 只表示停用记录有效。

配置器可以在尚未部署启动器时保存注册，注册成功本身不证明部署可用。接入程序应先完成第 7 节的发现与能力检查。注册无变更时允许 `committed:false` 且退出码为 0；通过 `list.data.entries` 核对自己的 `id`、`valid`、`enabled`。停用与移除只影响后续启动，不结束已运行的扩展。

注册记录不是信任凭证。不会从下载目录自动发现并执行清单，不会按清单中的 URL 下载代码。作者更新已受信任目录内的入口，属于该扩展自身的更新责任。

注册验证全图依赖。安装多个依赖时先注册被依赖项。注册记录损坏时不能覆盖猜测；可用 `list` 查看对应 ID，再检查 `CompanionRegistry\<ID>.json`。

注册目录超限或无法完整读取时，停用和移除仍按精确 ID 操作一份可确认归属的记录。返回 `registry-read-incomplete` 警告，退出码为 10；不能完整列出受影响依赖。损坏或归属不明的目标记录仍拒绝写入和删除。

停用、移除或降级被依赖项时，不级联修改其他扩展。操作结果以警告列出无法满足依赖的 ID，提交成功仍保留 `committed=true`；下次启动按 `skipped-dependency` 处理这些依赖项。

从旧目录迁移时，先移出同一入口的旧快捷方式，再启用注册项。能解析并确认同一入口时，注册返回 `legacy-entry-conflict`。注册后又添加同一旧入口时，本轮跳过旧入口并报告冲突。不能识别的包装脚本由用户核对，不按名称猜测或删除。

## 3. 入口与参数

| `runtime` | 入口 | 调用规则 |
| --- | --- | --- |
| `exe` | `.exe` | 明确程序路径，`CreateProcessW` |
| `powershell` | `.ps1` | 系统 Windows PowerShell `-NoProfile -File`；可显式指定受信任的 `hostPath` 使用 pwsh |
| `wscript` | `.vbs`、`.js`、`.wsf` | 系统 WScript；不按文件内容猜脚本语言 |
| `python` / `pythonw` | `.py` / `.pyw` | 本机 Python 宿主 |
| `node` | `.js`、`.cjs`、`.mjs` | 本机 Node 宿主 |
| `java` | `.jar` | 本机 Java 的 `-jar` |
| `batch` | `.bat`、`.cmd` | 现有固定 cmd 命令；参数只通过上下文传递 |

脚本运行环境缺失直接失败，不自动安装。`hostPath` 只对非 `exe`、非 `batch` 入口开放，必须是本机绝对 EXE 路径。缺失时复用既有宿主查找与单次缓存。PowerShell 不增加执行策略绕过参数。

清单参数是字符串数组，不是命令行模板。逐个按 Windows 参数规则编码，不解释 `%变量%`、`$()` 或 Shell 表达式。最多 128 项，每项最多 8192 个字符；最终 Windows 命令行必须小于 32767 个 UTF-16 单元，包含宿主、入口和结尾空字符。

非 batch 入口获得编码后的原生参数，同时可从上下文读取原数组。宿主自身的解析规则仍需测试；复杂脚本参数可直接读取上下文，避免再次拼接。

BAT/CMD 不拼接任意参数到 cmd 命令。原数组放在上下文 `input.arguments`。需要普通位置参数的旧批处理应保留普通启动方式，或提供遵守规范的适配脚本。

`.lnk` 继续走旧普通目录。需要健康确认的快捷方式应由作者提供明确入口或适配程序，不能同时从清单和快捷方式合并两套参数。

`entry.window` 可为 `normal` 或 `hidden`，默认 `normal`。`hidden` 仅控制新建控制台及启动时的窗口显示建议，不能保证第三方图形程序永不显示窗口。

## 4. 启动上下文与回执

### 传入环境变量

启动器为每个子进程构造独立环境块，不改系统环境。保留现有递归保护 `CHATGPT_PROXY_LAUNCHER_COMPANION=1`，新增：

| 变量 | 含义 |
| --- | --- |
| `CGL_CHAIN_PROTOCOL` | `1` |
| `CGL_CONTEXT_FILE` | 本次上下文 JSON 的绝对路径 |
| `CGL_LAUNCHER_PATH` | 本次实际使用的启动器路径 |
| `CGL_LAUNCHER_VERSION` | 启动器数值版本 |
| `CGL_RUN_ID` | 本轮启动随机 ID |
| `CGL_ENTRY_ID` | 扩展稳定 ID |
| `CGL_ATTEMPT` | 从 1 开始的尝试次数 |

变量仅说明本次启动来源。变量缺失不代表机器未部署启动器，独立启动时使用发现接口。

### 上下文

每次尝试使用独立目录和随机标识。以下为字段示意；尖括号字段在运行时填入真实值。

```json
{
  "schemaVersion": 1,
  "protocolVersion": 1,
  "runId": "<随机 ID>",
  "attemptId": "<随机 ID>",
  "token": "<256 位随机值的十六进制文本>",
  "deadlineUtc": "<UTC ISO 8601 时间>",
  "launcher": {"path": "<绝对路径>", "version": "<版本>", "deploymentState": "managed"},
  "entry": {"id": "org.example.assistant-addon", "version": "1.2.0", "attempt": 1},
  "input": {"arguments": ["--profile", "default"], "config": {}},
  "replyPath": "<本次回执绝对路径>",
  "chatgpt": {"processCreated": true, "readiness": "unknown"}
}
```

除下述短任务模式省略 `replyPath` 外，示例字段均必填。`deploymentState` 为 `managed`、`standalone` 或 `unknown`；不能把当前启动器副本误认为已正式部署。计时使用单调时钟，`deadlineUtc` 仅供扩展参考，修改系统时间不能延长等待。

临时目录位于当前用户临时目录内，使用随机名称和仅允许当前用户、SYSTEM 访问的 ACL。创建时检查路径、所有者和重解析点，不跟随已存在的重定向。上下文最多 64 KiB，先完整写入再创建进程。超长配置在启动前失败。

### 回执

扩展读取上下文，完成初始化，再把 UTF-8 JSON 回执写到 `replyPath + ".tmp"`，关闭文件后原子重命名到 `replyPath`。每次尝试只接受第一份有效回执。

```json
{
  "schemaVersion": 1,
  "runId": "<原样返回>",
  "attemptId": "<原样返回>",
  "token": "<原样返回>",
  "status": "ready"
}
```

前五个字段必填。回执不得超过 4 KiB；未知字段、重复键、错误标识或迟到回执不被接受。令牌用于区分旧回执，不是隔离恶意程序的安全边界。

| `status` | 额外字段与条件 |
| --- | --- |
| `ready` | 当前受跟踪根进程已就绪；无需 PID，使用启动时保留的句柄 |
| `already-running` | 必须提供 `readyProcessId` 和 `readyProcessCreationTime`；扩展已与自己的现有实例完成健康确认 |
| `failed` | 必须提供 `errorCode` 和布尔值 `retryable`；可选 `message`，最多 256 个字符 |

`readyProcessCreationTime` 是该进程创建时间 FILETIME 的 16 位十六进制文本。启动器打开进程并核对创建时间、当前用户和会话，避免只按 PID 误认。无法核对则失败。现有实例的健康状态由扩展负责确认，不能只搜索进程名就返回 `already-running`。

`message` 不含令牌、代理密码、完整参数或敏感配置。启动器将其作为纯文本处理。`errorCode` 使用稳定小写短码，不能按翻译后的消息判断重试。

### 两种确认方式

- `signal`：要求 `ready-file-v1`。收到 `ready` 后继续观察根进程 1 秒；收到 `already-running` 后观察已核对的实例 1 秒。若期间退出则失败。该 1 秒计入本次超时。
- `exit-code`：用于执行完即退出的短任务。等待根进程结束，退出码命中 `readiness.successExitCodes` 才记为 `completed`，默认 `[0]`。启用 Job 跟踪时还等待进程族归零；超过期限仍有子进程则失败。任务必须自己等待其实际工作结束；未跟踪的入口返回 0 不能证明后台工作完成。

`exit-code` 入口也获得上下文，但没有回执路径，不写回执。其上下文省略 `replyPath`，其余字段相同。不支持 JSON 的 VBS 或批处理可使用短任务模式，或由作者提供轻量适配器。

包装程序需要长期运行时，使用 `signal` 并保留根进程至就绪检查结束。包装程序转交现有实例后退出时，使用 `already-running`。不得靠“父进程返回 0”伪装服务已就绪。

尝试结束后，扩展不得再写协议目录。启动器只删除本次创建的上下文、回执及临时回执文件，再删除已空的目录。扩展自行及时读取所需信息；不把此目录当作长期存储。强制结束启动器时可能残留协议临时文件，v1 不增加后台清理服务。

## 5. 顺序、总预算与重试

### 调度

1. 取得本用户链式启动锁；已被另一轮占用时跳过本轮扩展，不能重复派发。此锁与部署写锁分开。
2. 读取注册项，验证大小、版本、必需能力、入口和完整依赖图。本次使用单次快照，不在每个状态切换时重新读取全部配置。
3. 普通目录先派发一次，再按稳定 ID 顺序执行注册图的拓扑排序。
4. 依赖达到 `ready`、`already-running` 或 `completed` 才启动后继。
5. 失败或超时的后继标记为 `skipped-dependency`，独立分支继续。
6. 总预算到期后不再启动新项，汇总结果，释放句柄并退出。

普通目录不能作为规范依赖。全部启用项构成的依赖图出现环时，不执行注册图；仍允许旧普通目录启动，并报告 `invalid-graph`。个别清单无效、缺依赖、依赖被停用或最低版本不满足时，跳过受影响分支，其他有效分支继续。

`companion-policy.json` 为 `{ "schemaVersion":1, "budgetMs":60000, "feedback":"notify" }`。预算允许 1000–120000 ms；反馈为 `notify` 或 `quiet`。仅在文件不存在时采用示例默认值。文件损坏或无法读取时，禁用规范注册图并报告 `invalid-policy`。整份配置通过校验后才生效，不使用部分字段。旧目录仍按默认预算运行。此文件最多 4 KiB，拒绝未知字段、重复键或不支持的版本。v1 可由用户按示例编辑，未来图形入口使用同一结构。

每项超时默认 15000 ms，上限 30000 ms。重试间隔默认 1000 ms，范围 250–5000 ms。全链最多 32 个注册项。总预算覆盖旧目录派发、注册图读取、就绪观察及重试间隔；每项有效等待时间取其超时与剩余总预算的较小值。旧目录不等就绪，但派发前也检查剩余预算。

超过 32 个注册文件时停止读取注册图并报告 `entry-limit`，不能按目录枚举顺序截取部分项。没有待启动项时不创建上下文；显式结果入口返回空 `entries`。

预算使用单调时钟。它限制主动等待，无法保证磁盘或系统 API 在相同时间内返回。不承诺硬实时截止。

### 可以重试的必要条件

默认只尝试一次。只有以下条件全部满足才重试：

- `retry.safe=true`，并声明必需能力 `tracked-retry-v1`。
- 用户有效尝试上限尚未用完，总预算足够。
- 错误属于可恢复错误；如果收到 `failed`，其 `retryable` 必须为 true。
- 上一次根进程及其受跟踪子进程已经全部退出，且跟踪状态可确认。

入口不存在、宿主缺失、配置无效、能力不支持、权限不足均不重试。创建进程前的暂时资源错误可按小范围错误码白名单重试；不得把全部 Windows 错误视为暂时故障。

超时后仍有进程运行，标记 `timeout-running`。不能结束它再重试。进程状态无法读取时，标记 `tracking-unknown`，也不重试。无回执且进程族已退出，可按声明和上限重试；退出码 0 本身不能替代 signal 回执。

扩展还须自己处理单实例复用。启动器锁只防止同时进行的启动链，不保证几分钟后再次启动时没有旧实例。

### 进程族跟踪方案

允许重试的入口在进程创建时加入独立 Job Object。使用 `PROC_THREAD_ATTRIBUTE_JOB_LIST`，避免先运行再附加导致漏掉早期子进程。不设置 `KILL_ON_JOB_CLOSE`、终止限制或允许脱离的标志。关闭启动器不终止扩展。[创建时附加 Job 的接口](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute)、[Job Object 行为](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)。

进程族是否为空通过 Job 的 `ActiveProcesses` 计数确认，不把等待 Job 句柄当作普通“所有进程退出”检测。根进程已退出但进程族尚未归零时，最多每 250 ms 复查一次，并受截止时间约束。跟踪创建或附加失败时报告错误，不悄悄改成不跟踪的重试。

Job 不能追踪所有服务、WMI 或外部代理代为创建的进程。声明 `safe=true` 的扩展不得通过这些途径脱离跟踪后再请求重试。需要这类行为的扩展只能尝试一次，并自行去重。[跟踪范围与例外](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)。

## 6. 失败反馈与资源占用

- 普通启动的 `feedback=notify`：失败时只汇总一次，显示失败扩展及原因。提示 10 秒后自动关闭，不使用阻塞式 MessageBox。提示不提供无限次重试按钮。
- `feedback=quiet`：不创建提示窗口。需要逐项结果时使用下面的显式结果入口；v1 不新增常驻状态服务或自动日志。
- 没有交互桌面时跳过通知。插件自己的弹窗不属于启动器可控制的范围。
- 通知最多在全链结束后额外占用 10 秒。启动链成功时没有通知等待。
- 普通启动的 ChatGPT 返回码保持原义；扩展失败不冒充 ChatGPT 启动失败。不将私人配置写入自动日志。
- 上下文只在注册项需要时创建。复用一次宿主查找结果；保留必要句柄，不按 10 ms 高频轮询全系统进程。不增加定时后台任务。
- 就绪回执优先使用目录变化通知，结合进程句柄和截止时间等待。无事件时没有持续刷新；实现阶段验证失败回退路径也不会忙等。

### 显式取得本轮结果

自动化调用方可使用独立入口：

```text
ChatGPTProxyLauncher.exe --launcher-chain --result <新文件绝对路径> --feedback quiet -- <ChatGPT 原始参数>
```

仅当第一参数完全匹配 `--launcher-chain` 时解析控制参数。`--feedback` 可为 `quiet` 或 `notify`，缺失时使用全链设置。必须提供 `--result` 和独立的 `--`。分隔符后的原始文本直接交给原启动路径，不拆分重组 ChatGPT 参数。

此入口仍启动 ChatGPT，然后运行同一启动链。结果文件在启动前独占保留，规则与部署结果一致；输出不可用时不启动。只对已确认支持 `chain-run-result-v1` 能力的启动器调用，不能探测旧 EXE。

结果包含 `schemaVersion:1`、`operation:"chain-run"`、`exitCode`、`chatgpt:{processCreated,nativeError}`、`entries` 和 `chainStatus`。每项只含 `id`、`status`、`code`、`attempts`、`elapsedMs`，不含原始参数、回执令牌或扩展配置。旧目录项使用 `legacy:<文件名>` 标识，状态最多到 `started-unverified`。

本入口退出码为：0 表示 ChatGPT 创建成功且全链无失败；10 表示 ChatGPT 创建成功但存在失败、跳过或预算耗尽；20 表示请求无效；50 表示 ChatGPT 创建失败；70 表示结果输出失败。`chainStatus` 为 `completed`、`partial`、`skipped-busy` 或 `not-started`。普通启动保留旧返回码，不受此接口影响。

扩展最终状态为 `ready`、`already-running`、`completed`、`started-unverified`、`failed`、`timeout-running`、`tracking-unknown`、`skipped-dependency`、`skipped-budget` 或 `disabled`。`disabled` 是主动停用，不单独导致退出码 10。`code` 进一步说明 `invalid-graph`、`runtime-missing` 或 `tracking-unavailable` 这类原因。

## 7. 对外发现与兼容

### 先区分三个问题

- 本次是否由协议启动：读取 `CGL_CHAIN_PROTOCOL` 和上下文。缺失时可独立运行，不据此自动安装。
- 本用户是否已有可用部署：使用可信配置器的 `--headless info`。没有本地配置器时使用下载脚本的 `-Headless -Operation info`。
- 本次启动器是否就是管理中的那一份：读取上下文 `launcher.deploymentState`。其取值为 `managed`、`standalone`、`unknown`，与下面六种发现状态不是同一组枚举。

`discovery.json` 位于当前用户固定配置根目录，是快速发现线索。文件不存在不能证明未部署，旧式部署可能只有管理快捷方式。文件存在也不能证明有效，须核对管理记录、目标、实际数值版本和能力。管理标记和版本资源不替代文件来源校验。

### 查询结果形状

配置器的结果包是 `schemaVersion:1, operation:"info", ...`，发现对象位于 `data`。结构如下，只有 `managed` 状态包含经校验的 `discovery`：

```json
{
  "schemaVersion": 1,
  "state": "managed",
  "launcherPath": "D:\\Apps\\Launcher\\ChatGPTProxyLauncher.exe",
  "launcherVersion": "1.1.0",
  "discovery": {
    "schemaVersion": 1,
    "managedBy": "ChatGPTProxyLauncher",
    "launcherPath": "D:\\Apps\\Launcher\\ChatGPTProxyLauncher.exe",
    "launcherVersion": "1.1.0",
    "setupApiVersions": [1],
    "chainProtocolVersions": [1],
    "capabilities": ["headless-deploy-v1", "companion-registry-v1", "ready-file-v1", "tracked-retry-v1", "chain-run-result-v1"]
  }
}
```

`launcherPath` 和 `launcherVersion` 在无法确定时可能为空。调用方先判断状态，再访问可选发现字段。完整包装结构、退出码和输出约束见[后台接口](automation-design.md#3-统一结果)。

| `data.state` | 处理规则 |
| --- | --- |
| `managed` | 检查发现格式、API 版本、链协议版本及所需能力；不代表 ChatGPT 已安装或运行 |
| `not-deployed` | 未找到管理快捷方式；经用户授权后可预检、部署、重新查询 |
| `legacy` | 有管理目标但缺少发现记录；作为兼容性检查或升级处理，不能当作全新安装 |
| `missing` | 管理目标文件缺失；作为明确的修复操作处理 |
| `invalid` | 记录损坏、版本不一致或归属不明；不自动覆盖 |
| `unknown` | 读取或权限错误；先解决问题，再查询 |

查询退出码为 0 只表示取得上述状态。`not-deployed` 仍可能遇到目标目录同名文件，部署保护不能被绕过。

### 没有部署时的接入顺序

按 `info → plan → 用户授权 → deploy → info → register → list` 接入。已有兼容部署时跳过计划和部署。初次使用复制[最小请求](examples/deploy-minimal-v1.json)，桌面和登录自启由用户选择。后台命令只传给配置器，不能传给启动器。完整代码见[自动化用法](automation-usage.md#通过-powershell-完成首次接入)。

### 已知兼容启动器的只读查询

`--launcher-info` 向重定向的标准输出写 UTF-8 JSON，不创建控制台、不开 ChatGPT。其输出是上面的发现对象本身，再附加 `currentLauncherVersion` 和 `currentCapabilities`；**没有配置器结果包，也没有 `data` 层**。前者描述管理部署，`current*` 描述当前运行的 EXE。

只对已确认来源和兼容版本（至少 v1.1.0）的启动器调用。不得仅看文件名或通过执行新参数来探测旧版。用进程 API 关闭 Shell、重定向 stdout，先读取输出、再等待进程退出并检查退出码；无可写 stdout 时返回 70。这个命令不会取得或恢复管理快捷方式中的私人启动参数。

### 能力与版本

能力名按行为版本固定：`headless-deploy-v1`、`companion-registry-v1`、`ready-file-v1`、`tracked-retry-v1`、`chain-run-result-v1`。扩展先检查 `chainProtocolVersions` 和必需能力，再决定是否注册。只检查 EXE 文件名或版本大小不足以证明能力存在。

`headless-deploy-v1` 和 `companion-registry-v1` 由配置器提供。其余三个由启动器提供。发现记录中的能力描述该部署对应的一组接口，不代表配置器被安装到部署目录；`--launcher-info` 的 `currentCapabilities` 只列当前启动器自己的能力。

`schemaVersion` 和 `protocolVersion` 独立于产品版本。旧版拒绝公共对象中的未知字段；变更字段或既有语义时升级受影响的格式版本。新可选行为用新能力名协商，不因产品版本升高就假定支持。扩展私有设置放入 `config`。新启动器继续读取已承诺兼容的旧协议，不把未知大版本当作 v1 猜测执行。公共版本、能力和限制维护在 `AutomationContract.h`，变更时同步 Schema 与契约测试。

规范 v1 不提供自动更新、持续心跳、远程健康 URL、服务安装或下载执行。扩展更新由作者负责。稳定清单路径、固定 ID 和独立用户覆盖项用于避免更新时重配参数。

## 8. 常见接入场景

以下约定使用现有 v1 字段，不代表新增了心跳、自动更新或停止进程接口。

### 桌面插件与已有实例

- 使用 `signal`。`ready` 应表示扩展的必要功能已可用；启动进程、显示窗口或发出连接请求都不够。ChatGPT 可能已经在运行，也可能尚未完成登录，扩展自行连接并在截止时间内判断结果。
- 单实例程序先通过自己的 IPC 与旧实例通信。需要更新参数时，先传递本次配置并取得确认，再返回 `already-running`；该回执不会替你转发参数或修改旧实例。
- 复用必须回报实际服务实例的 PID 和创建时间，不能只匹配进程名。新入口转交工作后可以退出；普通 `ready` 则观察根进程。
- 并发链会跳过重复派发，隔一段时间再次运行仍会重新尝试。长期实例自行做单实例保护；一次性副作用自行做幂等处理。v1 不保证“恰好执行一次”。

### 脚本、宿主与一次性任务

复制 [companion-task-v1.json](examples/companion-task-v1.json)，将入口替换为自己的脚本。使用 `exit-code`，完成工作后返回 `successExitCodes` 中的退出码；不要写回执，也不要在后台留下未完成的任务却先返回 0。

`powershell` 默认使用系统 Windows PowerShell。需要 PowerShell 7、指定 Python、Node.js 或 Java 时，由安装器检查运行环境，并在 `entry.hostPath` 填绝对 EXE 路径。入口及工作目录仍位于清单目录内。启动器不会安装宿主，也不绕过 PowerShell 执行策略。`batch` 参数从上下文读取，不拼接进命令字符串。

单项上限 30 秒，全链默认 60 秒。长时间任务不能靠提前报告成功避开限制；需要持续后台工作时，使用真正可服务的长期实例及 `signal`，重活由该实例继续执行。`hidden` 是显示建议，不是所有图形程序的静默保证。

### 多组件与依赖

用 `dependsOn` 声明必须成功的前置项，按依赖顺序注册。例如服务提供者先报告可用，界面再启动。普通 `StartupApps` 入口不能成为依赖；没有“可选依赖”字段，选装功能由扩展自己的配置处理。

依赖只证明本轮就绪或任务完成，不承诺后续持续存活。服务后续退出、断线和重连由消费者处理。不能用依赖顺序代替健康检查，也不能在扩展内再次调用启动器来启动后继。

### 配置、代理与运行目录

`entry.arguments` 是默认参数；`overrides.arguments` 是完整替换。`config` 是扩展私有 JSON 对象，可在内部设置自己的 `configVersion` 并实现迁移。作者决定配置字段及优先级，公共 v1 字段不能随意添加。

ChatGPT 的代理启动参数不会作为扩展参数传入；扩展会继承启动器现有环境，但不能据此认为它已获得 ChatGPT 的代理设置。需要代理时由用户明确配置扩展自己的 `config` 或参数。上下文不提供 `.env` 内容或 ChatGPT 私人启动参数。

上下文只用于本次尝试。初始化时读取并复制必要信息，长期配置放扩展自己的用户目录。不要保存回执令牌或将密码写入清单、参数及公开日志。独立启动时可使用扩展自己的配置；继承了协议变量但上下文已失效时，不得冒充新尝试或自行部署。

### 离线安装、更新与卸载

| 场景 | 调用方应做什么 |
| --- | --- |
| 普通安装器或插件市场 | 在目标用户会话内查询和注册；其他账户、系统服务账户的部署不代表该用户已安装 |
| 离线或便携发行 | 附带可信配置器及所需宿主；先查文件版本与能力，再走同样的本地接口。移动目录后显式更新注册路径 |
| 已有更高版本启动器 | 能力满足就直接注册；能力不满足则提示兼容性问题，不用安装器中的旧 EXE 覆盖它 |
| 扩展原目录更新 | 保持 ID 和清单路径，先准备完整文件，再切换；自行安排实例退出或延后更新，不覆盖正在使用的文件 |
| 扩展迁移目录 | 新目录准备完成后 `register --replace`，保留覆盖项；确认列表有效后再处理自己拥有的旧目录 |
| 扩展更新失败 | 恢复自己的文件和清单；若已更换注册路径，显式注册回原路径，并重新确认。注册接口不回滚扩展安装包 |
| 停用、卸载或恢复默认 | `configure` 整体替换覆盖项；空对象按作者默认值恢复并启用。`remove` 只移除自己的注册，不停进程、不卸载共享启动器 |
| 提权工具、服务或外部进程代理 | 不声明安全重试；Job 不能证明由其他服务代建的进程已退出。v1 不提供提权或服务安装接口 |

安装和注册分别提交，不是跨操作事务。注册失败时保留已有共享部署，报告原因，不为了回滚一个扩展而删除启动器。多个注册项也不支持一次原子提交，安装器自行跟踪已完成的步骤。

## 9. 测试要求

| 场景 | 预期 |
| --- | --- |
| 参数包含空字符串、中文、空格、引号、`& % !` | 参数边界不变；batch 不执行注入内容 |
| 回执及时、迟到、错误 token、旧尝试回执、超大回执 | 只接受本次期限内的有效回执 |
| ready 后立即退出、父进程退出但子进程仍活着 | 不误报长期正常，不重复创建残留进程 |
| 失败后进程族清空，声明安全重试 | 最多按有效次数重试，每次使用新 token |
| 无权限跟踪、Job 附加失败、进程状态未知 | 不降级成盲目重试 |
| 依赖环、缺依赖、依赖版本不足 | 受影响项不启动，结果原因准确 |
| 两轮并发、已有单实例、旧目录重复入口 | 不盲目重复派发，冲突可解释 |
| 无注册项、单项成功、超时、32 项预算耗尽 | 无常驻进程；等待和句柄有界 |
| 安装路径改变、清单升级、用户覆盖配置存在 | 固定 ID 和覆盖项保留，来源不被静默替换 |

测试使用隔离目录和测试子进程，不运行真实插件或 ChatGPT。就绪回执不能替代真实插件的互操作验收。

# 后台部署与扩展启动设计

状态：已实现，开发版 v1.0.7。日期：2026-11-14。实现前基线：`4f1d596`。

本地构建已提供下述接口。尚未打包或上传 Release。在线 PowerShell 命令须等新版脚本和支持后台部署的 EXE 发布后才能使用。操作示例见[自动化接入](automation-usage.md)。

## 1. 总体方案

继续使用两个原生 EXE，不增加常驻服务或第三个安装包。

- 配置器增加无界面入口。图形界面与无界面入口调用同一部署服务。
- PowerShell 负责下载、校验、传参和清理，不再实现另一套部署逻辑。
- 启动器先请求启动 ChatGPT，再运行有限时长的扩展启动流程。
- 扩展使用带版本的清单、参数数组和启动回执。详细约定见[链式启动规范](companion-protocol-v1.md)。

“后台部署”指部署期间不创建窗口、不抢焦点、不询问用户。它不等于隐藏 ChatGPT 或插件自身的界面，也不关闭调用者已有的终端。

### 本次明确的行为

1. 配置器有参数时先解析命令；错误不回退到界面。
2. `setup.ps1 -Headless` 返回一个对象，不显示 Star 提示、不打开网页、不退出调用者会话。
3. 旧启动目录保留单次派发。规范扩展使用独立的就绪、依赖和重试流程。
4. 现有退出 ChatGPT 的实现需要前台焦点。后台部署只报告重新打开应用的待办。

## 2. 部署器命令接口 v1

无参数时保留图形界面。第一参数为 `--headless` 时只走命令入口；解析失败也不能回退到界面。未知参数和未明确允许重复的参数报错。不能把拼错的后台命令当作 GUI 启动；有参数但不是受支持入口时直接返回 20。

```text
ChatGPTLauncherSetup.exe --headless deploy --request <绝对路径> --result <新文件绝对路径>
ChatGPTLauncherSetup.exe --headless plan --request <绝对路径> --result <新文件绝对路径>
ChatGPTLauncherSetup.exe --headless info --result <新文件绝对路径>
```

| 命令 | 行为 |
| --- | --- |
| `deploy` | 校验后部署并回读，使用现有同名文件保护和回滚机制 |
| `plan` | 只读检查，返回最终配置和拟修改项目，不部署、不建目录 |
| `info` | 只读查询部署状态、路径、版本和接口能力，不启动 ChatGPT |

`plan`、`info` 唯一允许的新文件是调用者指定的结果文件。二者不试写目标目录，不能保证稍后写入权限仍然有效。

扩展管理另有 `companion register/configure/remove/list` 子命令，具体参数见[链式启动规范](companion-protocol-v1.md#注册接口)。它们使用同一结果格式和写锁。

短命令也可使用以下参数。它们与 JSON 请求使用同一解析结果，不能与 `--request` 混用：

| 参数 | 含义 |
| --- | --- |
| `--base preserve\|defaults` | 继承现有配置，或使用无界面默认值；默认 `preserve` |
| `--directory <路径>` | 启动器部署目录 |
| `--startup on\|off` | 本工具的登录自启快捷方式 |
| `--desktop on\|off` | 本工具的桌面快捷方式 |
| `--proxy <URL>\|off` | HTTP、HTTPS 或 SOCKS5 地址，必须写出端口 |
| `--bypass <文本>` | 替换代理绕过规则；空文本表示清空 |
| `--extra-arg <单个参数>` | 可重复。整体替换额外参数数组，空参数也保留 |
| `--clear-extra` | 清空额外参数；不能与 `--extra-arg` 同用 |
| `--codex-env update\|leave` | 写入代理变量或保持原文件；默认 `leave` |
| `--official-startup disable\|leave` | 尝试关闭 ChatGPT 自带自启或保持原状；默认 `leave` |

`--extra-arg` 后的一个值属于参数内容，即使它以 `--` 开头。参数末尾缺值时直接报错。最终仍由既有参数校验器检查诊断参数、重复选项和长度。

### 请求文件

使用 UTF-8 JSON，最多 64 KiB，最多嵌套 16 层。允许开头的 UTF-8 BOM；输出统一不带 BOM。示例见 [deploy-v1.json](examples/deploy-v1.json)，结构见 [Schema](schemas/deploy-v1.schema.json)。

```json
{
  "schemaVersion": 1,
  "base": "preserve",
  "settings": {
    "startup": true,
    "desktop": true,
    "proxy": {"enabled": true, "protocol": "http", "host": "127.0.0.1", "port": 7897}
  },
  "actions": {"codexEnv": "update", "officialStartup": "disable"}
}
```

- `preserve`：未提供的启动设置保留。没有旧配置时使用下述无界面默认值。
- `defaults`：默认部署目录使用当前用户的本地应用数据目录；自启、桌面快捷方式和代理均关闭；额外参数为空。
- `actions` 是本次动作，不从旧配置或图形界面偏好中继承。无界面部署不修改图形界面的外观及 `.env` 开关偏好。
- `codexEnv=update` 必须同时有有效的最终代理配置。代理关闭时不得静默忽略这个请求。
- 关闭代理或选择 `leave` 不删除已有 `.env`。已有无效配置、未知字段、重复 JSON 键和不支持的版本均在写入前报错。
- 额外参数数组先按 Windows 参数规则编码，再交给 `BuildArguments`。继承的旧参数继续保留原文。文件路径要求本机完整路径；不展开 `%变量%` 或执行字符串模板。
- 代理 URL 不允许认证信息、查询或片段。JSON 的 `host` 是主机名或 IP，不含协议和端口；IPv6 使用不带方括号的地址。命令 URL 的 IPv6 按 URL 规则加方括号。沿用现有协议与主机校验，不把错误地址写入快捷方式。
- 代理 URL 的路径只允许空或 `/`。所有传入路径和进程参数拒绝 NUL。不得把 JSON Schema 验证通过当作路径、参数及权限检查已经通过。
- 不隐式检测代理端口。自动检测失败或识别到多个端口，不能成为无界面部署的默认猜测。
- 暂不可用的 ChatGPT 静默启动选项不新增命令开关；继承旧设置时保留其已有值。

### 执行次序和并发

```text
解析命令与结果路径 → 保留结果输出文件 → 读取请求并校验 → 获取本用户操作锁
→ 在锁内重新读取现有状态 → 生成最终计划 → 文件提交与回读
→ 可选的自带自启处理 → 写结果 → 释放句柄并退出
```

图形界面与无界面入口共用操作锁。界面打开但空闲时不占用写锁；保存、扩展注册和无界面部署时占用。锁等待最多 5 秒，超时返回 `busy`，不激活其他窗口。

`.env`、程序、快捷方式和新增的能力发现文件纳入文件提交与回滚。系统自带自启在文件提交成功后另行处理；失败只产生警告。部署不自动启动 ChatGPT，也不执行扩展清单。

无界面模式不调用需要抢焦点的 ChatGPT 退出逻辑。写入 `.env` 后，在结果中返回 `restart-chatgpt` 待办项。自带自启无法确认时返回 `check-official-startup` 待办项，不打开 Windows 设置。

## 3. 统一结果

所有命令使用同一结果结构，结果是 UTF-8 JSON。示例中的空对象表示省略的具体字段，并非字段值可以任意变化。

```json
{
  "schemaVersion": 1,
  "operation": "deploy",
  "status": "success-with-warnings",
  "exitCode": 10,
  "code": "deployed-with-actions",
  "committed": true,
  "stage": "post-actions",
  "nativeError": 0,
  "warnings": [],
  "actionsRequired": ["restart-chatgpt"],
  "data": {}
}
```

| 退出码 | 含义 |
| --- | --- |
| `0` | 本次操作成功，无待办；查询成功不代表已部署 |
| `10` | 部署成功，但有警告或需要用户后续操作 |
| `20` | 请求、路径、清单或参数无效 |
| `21` | 不支持接口版本或所需能力 |
| `30` | 同名文件或注册所有权冲突，未覆盖 |
| `31` | 其他操作正在占用写锁 |
| `40` | 文件提交失败，回滚完成 |
| `41` | 文件提交失败，回滚未完整完成，需要检查 |
| `50` | 预检或系统读取失败，尚未提交 |
| `70` | 结果文件无法创建或写入；不能据此推断是否已部署 |
| `71` | PowerShell 下载、版本门槛或校验失败，没有启动部署器 |
| `72` | PowerShell 未能取得可解析的部署结果；实际提交状态未知 |

`status` 只取 `success`、`success-with-warnings`、`error`。`operation` 为 `deploy`、`plan`、`info`、`companion-register`、`companion-configure`、`companion-remove` 或 `companion-list`。业务原因用稳定的 `code` 判断，原始 Windows 错误放在 `nativeError`。调用方不能按中文错误消息匹配分支。

`committed` 可为 `true`、`false` 或 `null`。全部文件提交成功为 true；只读操作、提交前失败、完整回滚为 false；部分回滚失败或无法取得结果为 null。它不代表应用重启或自带自启后续动作成功。

`warnings` 是 `{ "code":"...", "message":"..." }` 的数组。`actionsRequired` 使用固定动作名。`stage` 取 `parse`、`preflight`、`lock`、`commit`、`rollback`、`post-actions`、`result` 或 `transport`。`nativeError=0` 表示没有原始系统错误。

`data` 按操作划分：部署返回启动器路径和版本；plan 返回最终配置及变更列表；info 返回部署状态、发现信息及支持能力；扩展管理返回受影响 ID，list 返回注册项摘要。失败时不返回尚未验证的成功数据。

结果文件必须是以 `.json` 结尾的新文件。写入前先以独占方式创建，若已存在则拒绝；不能覆盖用户的配置文件。调用方等待进程退出后再解析，崩溃留下的空文件或半份 JSON 不算有效结果。最终结果写入失败返回 70，调用方改用 `info` 检查，不能无条件重试部署。

创建结果前核对规范路径，不得与请求文件、清单或管理目标重合，包括 `.env`、快捷方式、程序和发现文件；不能写进注册目录。结果文件父目录必须已存在。解析错误时，如果能取得有效 `--result` 则写错误结果；无法取得输出路径时只能通过进程退出码报告，不打开窗口。

常规结果不回显完整启动参数、代理地址、`.env` 内容或扩展配置。`plan` 返回调用方主动要求查看的最终配置，文件不得当作普通日志分享。

### 发现已部署启动器

部署时写入 `%LOCALAPPDATA%\ChatGPTProxyLauncher\discovery.json`，与部署结果一起回读。内容如下：

```json
{
  "schemaVersion": 1,
  "managedBy": "ChatGPTProxyLauncher",
  "launcherPath": "D:\\Apps\\ChatGPTLauncher\\ChatGPTProxyLauncher.exe",
  "launcherVersion": "1.0.7",
  "setupApiVersions": [1],
  "chainProtocolVersions": [1],
  "capabilities": ["headless-deploy-v1", "companion-registry-v1", "ready-file-v1", "tracked-retry-v1", "chain-run-result-v1"]
}
```

该文件是发现线索，不是真实性证明。查询仍要核对管理记录、文件存在状态和数值版本。旧版没有此文件时，只读现有管理快捷方式，返回 `legacy`，不能误报“未安装”。副本 EXE 存在、启动器已部署、ChatGPT 正在运行是三个不同状态。

能力列表只包含该版本已实现的接口。`launcherVersion` 来自数值版本资源：第四段为零时省略，因此通常为三段。不要依赖固定的文本段数，也不要把它当作扩展版本。

查询状态包括 `not-deployed`、`managed`、`legacy`、`missing`、`invalid`；启动器查询还可能返回 `unknown`。记录与 EXE 版本不匹配时返回 `invalid`，不会使用过期能力。

部署能力描述生成此记录时的配置器接口，不表示配置器 EXE 已留存在安装目录。部署命令始终调用配置器；扩展启动和查询命令始终调用启动器。外部工具需要再次部署时，应取得支持该接口的配置器或调用 PowerShell 下载入口。

给已知且受信任的启动器增加独立命令：`--launcher-info`。只在第一参数完全匹配且没有其他参数时消费它，向调用方重定向的标准输出写 UTF-8 JSON，不启动 ChatGPT、不修复快捷方式、不读取私人代理参数。不创建控制台；没有可写输出句柄时返回 70。普通启动参数的原样传递规则不变。旧启动器会把未知参数转发给 ChatGPT，所以外部程序必须先读发现信息确认支持，不能把这个命令当作旧版探测手段。

`--launcher-chain` 是另一个显式入口，用于请求本轮扩展结果文件。参数边界和退出码见[链式启动规范](companion-protocol-v1.md#显式取得本轮结果)。

## 4. PowerShell 入口

现有无参数 `irm ... | iex` 保留原交互流程。后台调用使用带参数的脚本块：

```powershell
$url = 'https://raw.githubusercontent.com/EthenGod/ChatGPT-Proxy-Launcher/master/setup.ps1'
$result = & ([scriptblock]::Create((irm $url))) -Headless -Request 'C:\Deploy\request.json' -Result 'C:\Deploy\result.json'
```

上面的远程命令需要对应代码已经发布。本地开发时先使用本地 EXE，不能据此调用旧脚本。

- 后台模式不显示进度条、提示窗口、Star 文本或网页，不等待按键。
- 不在调用者会话内执行 `exit`。返回一个结构化对象，不混入下载输出。外部任务需退出码时，由它自行执行 `exit $result.exitCode`。
- 先取得固定发布标签，再下载同标签的 EXE 和校验清单。最低 EXE 版本为 **1.0.7**。
- 校验通过后读取文件版本。低于门槛或无法识别时停止，不尝试调用它的 `--headless`。旧 EXE 会忽略参数并打开界面。
- 部署器通过 `-WindowStyle Hidden` 启动，脚本等待本次进程退出。隐藏窗口仅是辅助；真正无界面由 EXE 的命令入口保证。
- 原生结果先写入本次专属临时目录；PowerShell 等待结束后读取，并附加可选的 `transport` 字段。对外结果仍使用同一结构。
- 最终 `-Result` 路径在下载前保留，避免部署成功后才发现输出位置不能创建。它必须是新 `.json` 文件，且位于本工具配置根目录之外。仅在此时创建的文件上写结果。
- 清理本次下载的 EXE、校验清单及原生结果文件。请求文件和最终结果文件属于调用者，不删除。
- 部署成功但清理失败，返回 10 并保留 `committed=true`；部署本身失败则保留其失败码。部署器崩溃或结果损坏返回 72，`committed=null`。
- 网络或校验失败也返回结构化错误，不显示窗口。调用者强制关闭脚本导致的中断不保证产生结果或完成清理。

## 5. 模块

GUI 和后台共用现有文件提交核心，没有复制两套部署规则。

| 模块 | 责任 |
| --- | --- |
| `SetupCommandLine` | 命令入口、请求解析、结果序列化，不依赖窗口 |
| `SetupCore` | 共用预检、文件提交、所有权检查和回滚；两个入口使用同一用户写锁 |
| `AutomationJson` / `AutomationIO` | 有界 JSON、路径检查、参数编码、结果文件及用户锁 |
| `CompanionRegistry` | 注册、用户覆盖配置、只读发现及同名保护 |
| `CompanionManifest` | 清单校验、路径解析、版本及依赖检查 |
| `CompanionRunner` | 有限时长的调度、启动确认、重试和结果汇总 |
| `LauncherAutomation` | 发现查询、显式链式入口及限时失败提示 |

使用共用的小型原生 JSON 解析器，不引入运行库或 Schema 引擎。这样能在同一次解析中拒绝重复键、非法 Unicode、超深结构和整数精度丢失。与初稿相比，不再依赖 WinRT JSON。Schema 用于开发期检查，运行时还检查路径、所有权和配置之间的关系。

四部分均已接入，并分别使用隔离测试验证：

1. 无界面部署器和结果接口。先验证 GUI 和命令入口的计划一致。
2. PowerShell 后台分支。验证版本门槛、退出码、清理与调用者会话保留。
3. 扩展注册、清单、发现信息和启动回执。
4. 依赖调度、跟踪后的有限重试和错误提示。

规范、Schema、示例、新模块和测试均纳入源码发布清单。本次不生成 Release 附件。

## 6. 验收清单

- 后台分支在成功、参数错误、冲突和失败路径均不调用窗口、文件夹、网页、焦点或设置接口。
- 两个入口对同一最终配置生成相同快捷方式和 `.env`；后台默认不请求退出 ChatGPT。
- 对部署锁竞争、结果写入失败、每个提交点的失败和回滚失败分别注入测试。
- 覆盖空参数、中文、空格、引号、反斜杠、`& | % !`、重复 JSON 键、非法编码和长度边界。
- 原生结果和 PowerShell 对外结果语义一致。Windows PowerShell 5.1、PowerShell 7 都能调用；旧 EXE 不能误弹界面。
- 按链式规范测试迟到回执、残留子进程、重复运行、依赖失败、宿主缺失及未知协议。
- 对照现版测量空扩展目录、单项成功、失败重试和总预算耗尽。时间、峰值内存和句柄数量分开记录，不把插件自身开销算作启动器优化。
- 性能验收目标：同机暖启动至少 100 次，空扩展场景的 p50 增量不超过 5 ms、p95 增量不超过 10 ms；峰值私有内存增量不超过 1 MiB。已测范围和限制见[实现审查记录](automation-review.md)。机器噪声超出阈值时先重复对照，不用一次采样判定。
- 等回执期间优先用内核等待，30 秒无事件场景的启动器 CPU 时间增量目标不超过 100 ms。结束后不残留启动器进程、测试线程或句柄；已正常启动的扩展继续运行。

## 7. 默认策略

后台部署不自动关闭 ChatGPT。每个扩展默认只启动一次。安全重试最多三次尝试。全链等待预算默认 60 秒。失败提示在全链结束后最多再显示 10 秒；`quiet` 不显示提示。

## 8. 验证记录

初稿的两个示例及 Schema 完成 25 项结构检查。实现阶段补上真实隔离子进程、文件提交和回滚、用户锁、回执时序及 PowerShell 替身测试。

当前验证范围和复测命令见[实现审查记录](automation-review.md)。真实插件互操作、登录自启及 ChatGPT 网络状态须另做实机验收；不能由隔离测试推断。

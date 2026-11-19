# 后台部署接口 v1

原生接口自 v1.1.0 提供。接入顺序见[开发者指南](developer-guide.md)，可复制命令见[自动化用法](automation-usage.md)。本文只定义命令、请求、结果和实现分工。

## 1. 运行边界

配置器负责部署和注册，启动器负责启动 ChatGPT 及扩展。两者均为原生程序，不常驻。GUI 与后台共用文件提交和回滚代码；PowerShell 只负责下载、校验、转发和清理。

后台模式不创建窗口、不抢焦点、不打开设置或网页，也不退出 ChatGPT。参数错误不会回退 GUI。调用方负责事先取得用户选择和传达结果中的待办。

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

扩展首次接入优先使用 [deploy-minimal-v1.json](examples/deploy-minimal-v1.json)：保留已有启动设置，两项系统动作均为 `leave`。下面的完整示例会开启代理、自启、桌面图标并更新 `.env`，仅用于用户已选择这些设置的场景。

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
| `10` | 操作完成，但有警告或需要用户后续操作 |
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

`committed` 可为 `true`、`false` 或 `null`。文件成功提交为 true；只读操作、无变更的注册/移除、提交前失败、完整回滚为 false；部分回滚失败或无法取得结果为 null。不能把 `false` 单独当作失败。它不代表应用重启或自带自启后续动作成功。

`warnings` 是 `{ "code":"...", "message":"..." }` 的数组。`actionsRequired` 使用固定动作名。`stage` 取 `parse`、`preflight`、`lock`、`commit`、`rollback`、`post-actions`、`result` 或 `transport`。`nativeError=0` 表示没有原始系统错误。

`data` 按操作划分：部署返回启动器路径和版本；plan 返回最终配置及变更列表；info 返回部署状态、发现信息及支持能力；扩展管理返回受影响 ID，list 返回注册项摘要。失败时不返回尚未验证的成功数据。

结果文件必须是以 `.json` 结尾的新文件。写入前先以独占方式创建，若已存在则拒绝；不能覆盖用户的配置文件。调用方等待进程退出后再解析，崩溃留下的空文件或半份 JSON 不算有效结果。最终结果写入失败返回 70，调用方改用 `info` 检查，不能无条件重试部署。

创建结果前核对规范路径，不得与请求文件、清单或管理目标重合，包括 `.env`、快捷方式、程序和发现文件；不能写进注册目录。结果文件父目录必须已存在。解析错误时，如果能取得有效 `--result` 则写错误结果；无法取得输出路径时只能通过进程退出码报告，不打开窗口。

常规结果不回显完整启动参数、代理地址、`.env` 内容或扩展配置。`plan` 返回调用方主动要求查看的最终配置，文件不得当作普通日志分享。

### 发现已部署启动器

部署时写入 `%LOCALAPPDATA%\ChatGPTProxyLauncher\discovery.json`。`info` 会核对管理快捷方式、目标文件、数值版本及发现记录；不读取私人启动参数。结构、六种状态及 `--launcher-info` 的区别统一见[发现规范](companion-protocol-v1.md#7-对外发现与兼容)。

`launcherVersion` 来自数值版本资源，第四段为零时省略，不应假定固定段数。能力描述对应的一组接口，不代表配置器 EXE 被安装到部署目录。

新桌面及登录自启入口名为 `ChatGPT Launcher.lnk`。保存时识别并迁移旧 `ChatGPT Proxy Launcher.lnk`，只操作管理标记和目标均匹配的链接。`plan.data.changes` 会列出旧路径的 `remove-if-managed`；同一个 `item` 可对应多个路径，不能按 `item` 去重。EXE 文件名、配置根目录和机器标记保持兼容。

## 4. PowerShell 入口

无参数调用进入 GUI；后台必须传 `-Headless -Result <新 JSON 路径>`。操作和对应参数以[命令表](automation-usage.md#本地配置器命令)为准。`companion-*` 转发已有的原生 v1 接口；线上入口须先发布更新后的脚本。

- 参数组合、路径、请求或清单的 64 KiB 上限在下载前检查；JSON 内容和所有权由原生接口校验。
- 最终结果在下载前独占创建，必须位于固定管理目录之外。请求、清单、结果和调用者会话均保留；输入错误可能只有返回对象。
- 先取固定 Release 标签，再下载同标签的 EXE 与校验清单。先校验 SHA-256 和最低 EXE 版本 1.1.0，再启动；不能用新参数试探旧版。
- 隐藏启动配置器，等待进程退出后读取内部结果，返回一个结构化对象，不混入进度、Star 提示或网页。外部调度器需要退出码时，由最外层执行 `exit $result.exitCode`。
- 清理自身下载文件及内部结果。清理失败但操作成功时返回 10，保留提交状态；原生操作失败则保留失败码。崩溃或结果无效返回 72，`committed:null`。强制中断不保证清理或结果完整。
- 每次调用均下载，不维护缓存。连续调用、离线安装和频繁查询应使用可信的本地配置器副本。

## 5. 模块

GUI 和后台共用现有文件提交核心，没有复制两套部署规则。

| 模块 | 责任 |
| --- | --- |
| `SetupCommandLine` | 命令入口、请求解析、结果序列化，不依赖窗口 |
| `SetupCore` | 共用预检、文件提交、所有权检查和回滚；两个入口使用同一用户写锁 |
| `AutomationJson` / `AutomationIO` | 有界 JSON、路径检查、参数编码、结果文件及用户锁 |
| `AutomationContract` | 公共协议版本、能力表、限制及状态分类 |
| `DeploymentDiscovery` | 两个 EXE 共用的发现信息生成、只读查询和版本校验 |
| `CompanionRegistry` | 注册、用户覆盖配置及同名保护 |
| `CompanionManifest` | 清单校验、路径解析、版本及依赖检查 |
| `CompanionProtocol` | 纯回执解析；完整校验后才返回，不操作进程 |
| `CompanionRunner` | 有限时长的调度、启动确认、重试和结果汇总 |
| `LauncherAutomation` | 发现查询、显式链式入口及限时失败提示 |

JSON 解析器统一检查大小、重复键、编码、深度和整数精度。Schema 用于开发期检查；路径、所有权和字段关系仍由原生代码校验。

## 6. 修改与验证

接口变更同步规范、Schema、示例、契约测试和 `tools/release-sources.ps1`。字段或既有语义不兼容时升级相应格式版本；扩展私有设置放 `config`。

运行 `cmd /c tests\test-build.cmd`，修改原生代码后再运行 `cmd /c tests\analyze.cmd`。测试使用隔离文件、替身系统调用和测试子进程。历史结果见[维护记录](automation-maintenance.md)，性能方法见[性能审查](performance-review.md)；通过测试不代表实际登录自启或第三方插件互操作成功。

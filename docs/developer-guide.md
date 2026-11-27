# 开发者接入指南

面向希望随 ChatGPT 启动的程序、脚本和插件作者。适用于 v1.1.0 及后续兼容版本，接口仍为 v1。

接入分两部分：**安装时查询、按需部署和注册；运行时读取上下文、初始化并报告状态**。不要每次启动插件都下载配置器、部署或注册。

可复制的 PowerShell 全流程见[自动化用法](automation-usage.md)。完整字段见[后台接口](automation-design.md)和[链式启动规范](companion-protocol-v1.md)。

## 1. 先选接入方式

| 扩展类型 | 选什么 | 扩展需要做什么 |
| --- | --- | --- |
| 普通程序，不准备改代码 | 配置器高级页“添加程序” | 沿用 `StartupApps`；不提供健康确认或自动重试 |
| 持续运行的插件、桌面程序 | `signal` | 初始化成功后写一次回执，继续运行 |
| 执行完即退出的脚本或任务 | `exit-code` | 等实际工作完成后返回退出码，不写回执 |
| 已有实例的单实例程序 | `signal` + `already-running` | 先与旧实例确认健康，再回报其 PID 和创建时间 |

本项目只部署启动器，不安装 ChatGPT 或脚本运行环境。部署、注册都不会启动 ChatGPT。扩展不能把“创建 ChatGPT 进程成功”当作“ChatGPT 已登录或网络可用”。

## 2. 安装时先查询，不靠文件名猜

使用可信的配置器调用 `--headless info --result <新 JSON 文件>`，或通过下载脚本调用 `-Headless -Operation info`。**本机没有配置器或启动器时，下载入口仍可查询和部署**。

本地 EXE 要先核对来源及数值文件版本至少为 1.1.0，再调用后台命令。不要向旧 EXE 发送新参数试探支持情况。配置器不会随启动器长期留在部署目录中。

读取 `report.data.state`：`managed` 检查能力后注册；`not-deployed` 进入下一节；`legacy`、`missing` 按明确的升级或修复处理；`invalid`、`unknown` 停止自动写入。[完整状态表](companion-protocol-v1.md#查询结果形状)解释各状态。不要把所有非 `managed` 状态当成未安装。

对于 `managed`，检查 `report.data.discovery`：

- `schemaVersion == 1`，`setupApiVersions` 包含 1，`chainProtocolVersions` 包含 1。
- `capabilities` 包含 `companion-registry-v1`，以及作者清单 `requires` 中的每项能力。
- 需要程序化取得一次启动结果时，另检查 `chain-run-result-v1`。

能力不够时提示升级，不按产品版本大小猜测能力。字段位置和返回形状见[发现接口](companion-protocol-v1.md#7-对外发现与兼容)。

## 3. 没有部署时，使用最小请求

复制 [deploy-minimal-v1.json](examples/deploy-minimal-v1.json)。不要每次启动扩展都部署；安装器也不能用自己附带的旧配置器覆盖更高版本。

首次部署时会写入启动器、管理快捷方式和发现信息；默认不启用代理、不建桌面和登录自启快捷方式。已有配置时保留设置。注册目录与程序文件互相独立。

1. 向用户说明要安装什么、部署到哪里。授权发生在你的安装流程中，后台接口不会再弹确认框。
2. 用此请求运行 `plan`，检查最终目录和 `data.changes`。计划只检查，不保证稍后的磁盘和权限状态。
3. 用户已经认可最终设置后，运行 `deploy`。
4. 检查退出码、`committed`、`warnings` 和 `actionsRequired`。成功后重新 `info`，再次检查协议能力，然后注册。

需要桌面入口或 Windows 登录自启时，在用户选择后显式设置 `settings.desktop` 或 `settings.startup`。代理和 `.env` 是额外功能，不是链式启动前提。[完整部署示例](examples/deploy-v1.json)会修改这些项目，不应直接当作默认安装请求。

退出码 41、70、72 或 `committed:null` 时，先查询实际状态，不循环重试部署。未知归属的文件不能靠删除来绕过保护。恢复方法见[结果处理](automation-usage.md#结果与恢复)。

## 4. 给扩展添加清单

将 `companion.json` 放在扩展的稳定安装目录，入口与清单一起升级。不要从会自动清理的临时下载目录注册。最小常驻 EXE 清单为：

```json
{
  "schemaVersion": 1,
  "id": "org.example.assistant-addon",
  "name": "Example Addon",
  "version": "1.0.0",
  "requires": ["ready-file-v1"],
  "entry": {"runtime": "exe", "path": "Addon.exe", "arguments": []},
  "readiness": {"mode": "signal", "timeoutMs": 15000}
}
```

ID 使用稳定的小写反向域名，不含版本号。入口是清单目录内的相对路径。支持的脚本类型、宿主和参数规则见[入口与参数](companion-protocol-v1.md#3-入口与参数)。

初次接入只尝试一次。可复制的[完整清单](examples/companion-v1.json)也默认关闭重试。只有通过失败清理和单实例测试后，才考虑声明安全重试。

## 5. 注册并确认入口

使用配置器的 `companion register`，或下载脚本的 `-Operation companion-register -Manifest <绝对路径>`。注册会校验并保存记录，**不会马上运行插件**。

注册后用 `companion list` 核对自己的 ID，要求 `valid:true` 和 `enabled:true`。同 ID、同路径重复注册会保留用户覆盖项；可能没有新写入，此时 `committed:false` 不是失败。不要只看这个布尔值。

用户下次通过本启动器或它创建的快捷方式打开 ChatGPT 时，扩展才会启动。从 ChatGPT 原始入口打开不会触发本链。直接运行不带参数的启动器，也不会自动加载管理快捷方式中保存的代理参数；正常使用应走已配置的快捷方式。

不要同时把同一入口放进 `StartupApps`。需要迁移时让用户或安装器处理自己拥有的旧入口，再注册。

## 6. 扩展运行时报告状态

1. 检查 `CGL_CHAIN_PROTOCOL=1` 和 `CGL_CONTEXT_FILE`。变量缺失只说明本次不是协议启动，**不代表启动器未部署**。不要在这里反复安装或递归启动启动器。
2. 读取 UTF-8 上下文，核对版本及本次身份，保存实际需要的参数和配置。文件只在本次尝试期间存在。
3. 使用 `input.arguments` 和 `input.config` 初始化真正的功能。需要连接 ChatGPT 时自行等待连接，但不得超过上下文截止时间。
4. `signal` 模式在初始化成功后写一次 `ready`。启动器继续观察关联进程 1 秒；这 1 秒计入默认 15 秒超时。
5. 初始化失败时发送 `failed`，清理自身资源并退出。配置错误不能请求重试；不要在报告失败后留着一半运行的实例。
6. `exit-code` 模式没有 `replyPath`，不调用回执函数，等待任务完成后按成功或失败退出。

PowerShell 扩展可分发并点调用 [companion-context.ps1](examples/companion-context.ps1)：

```powershell
. (Join-Path $PSScriptRoot 'companion-context.ps1')
$context = Read-CglContext
# 使用 $context.input 完成实际初始化；下面这行只能放在初始化成功后。
Send-CglReply -Context $context -Status ready
# 接着进入扩展自己的正常主循环，不要立即退出。
```

已知配置错误可调用 `Send-CglReply -Context $context -Status failed -ErrorCode invalid-config`，随后退出。复用已有实例前先确认它实际可用，再使用 `-Status already-running -ExistingProcessId <PID>`。

其他语言只需要环境变量、UTF-8 JSON 和文件操作：原样返回 `runId`、`attemptId`、`token`，先写 `reply.json.tmp` 并关闭，再同目录重命名到 `replyPath`，禁止覆盖已有回执。完整字段和限制见[上下文与回执](companion-protocol-v1.md#4-启动上下文与回执)。

## 7. 配置、升级和卸载

使用 `companion configure` 修改配置或停用；它整体替换 `overrides`，不是补丁合并。原目录升级保留 ID 和清单路径；换目录使用 `register --replace`；卸载只 `remove` 自己的 ID。[维护命令](automation-usage.md#配置停用和移除)可直接复制。

按需阅读[应用场景](companion-protocol-v1.md#8-常见接入场景)：已有实例、脚本宿主、一次性任务、多组件依赖、配置与代理、离线安装，以及升级回滚。移除注册不会停止正在运行的扩展，也不应删除共享启动器。

## 8. 接入完成前检查

- 未部署、兼容部署、旧版、文件丢失、权限失败都进入正确分支；未经用户选择不改代理、自启或 `.env`。
- 带空格和中文的路径、空参数、引号都能正确传递；使用参数数组，不经 Shell 拼接用户输入。
- 首次注册、重复注册、停用、升级和移除均有效；错误码 10 的警告没有被丢弃。
- 就绪、初始化失败、超时、已有实例都能得到准确结果。声明重试前验证整个上一轮进程族已退出。
- 验收显式链式结果时，确认 `chatgpt.processCreated` 以及自己的条目状态。该测试会真的启动 ChatGPT，不是只读检查。
- 不把令牌、上下文、代理或完整参数写进公开日志。启动器退出后没有持续健康检查。

## 修改本项目时看哪里

仓库目录和程序入口见下方的[项目地图](#项目地图)。后台接口的公共版本、能力和限制统一维护在 [AutomationContract.h](../src/AutomationContract.h)。

接口变更同步 [Schema](schemas/)、[示例](examples/)和文档。新增公共字段不能直接写入 v1。扩展私有设置优先放 `config`。接口校验与已知边界见[维护记录](automation-maintenance.md)。

新增源码、测试或文档时，更新 `tools/release-sources.ps1`。文档引用的本地文件也要随源码 ZIP 交付。编译、静态分析、完整测试和实际窗口验收的顺序见[开发命令](ui-refactor.md#开发命令)。

## 项目地图

目录树从仓库根目录起算，列出 Git 跟踪的目录。目录增删或移动时，用 `git ls-tree -dr --name-only HEAD` 核对已提交的结构。单个目录的完整文件清单可用 `git ls-files -- src/` 查看，把 `src/` 换成要查的目录即可。

```text
.
├── assets/                 Windows 程序图标及来源说明
├── docs/                   接入指南、设计与维护记录
│   ├── examples/           部署请求、扩展清单和回执脚本示例
│   └── schemas/            后台部署和扩展协议的 JSON Schema
├── frontend/               配置器的前端工程和桌面构建脚本
│   ├── .cargo/             Windows x64 MSVC 的 Rust 编译选项
│   ├── src/                React / TypeScript 页面
│   │   ├── assets/         附加程序示例图标
│   │   ├── components/     共用控件、滚动和外观过渡
│   │   ├── features/
│   │   │   └── setup/      设置页、参数预览和附加程序列表
│   │   ├── i18n/           前端语言选择与文本回退
│   │   ├── platform/       桌面窗口接口、原生服务和模拟服务
│   │   └── styles/         设置页布局和主题样式
│   └── src-tauri/          Rust 桌面外壳、Tauri 配置和构建脚本
│       ├── capabilities/   主窗口权限
│       └── src/            配置器入口、任务队列和原生调用
├── locales/                语言登记表、翻译文本和校验规则
├── src/                    C++ 启动器、部署核心、兼容界面和 Windows 资源
├── tests/                  原生测试、隔离窗口检查及测试脚本
│   ├── fixtures/           供测试使用的设置快照
│   ├── frontend/           前端组件和接口测试
│   └── rust/               Rust 桥接契约和内嵌资源测试
└── tools/                  原生构建、翻译生成和源码包清单
```

| 要改的部分 | 从哪里读起 |
| --- | --- |
| 启动 ChatGPT | [ChatGPTProxyLauncher.cpp](../src/ChatGPTProxyLauncher.cpp) 查找应用、处理启动参数并创建进程。普通附加程序由 [StartupApps.cpp](../src/StartupApps.cpp) 读取，注册扩展的调度见[后台接口模块表](automation-design.md#5-模块)。 |
| 修改设置页 | [main.tsx](../frontend/src/main.tsx) 接入原生服务，[prototype.tsx](../frontend/src/prototype.tsx) 接入内存模拟服务。两者渲染同一个 [SetupPage.tsx](../frontend/src/features/setup/SetupPage.tsx)。 |
| 修改配置器的原生行为 | [main.rs](../frontend/src-tauri/src/main.rs) 分流后台命令、兼容界面和默认窗口。[ChatGPTLauncherSetup.cpp](../src/ChatGPTLauncherSetup.cpp) 实现兼容界面。默认窗口通过 [Rust 桥接层](../frontend/src-tauri/src/bridge.rs) 调用 [C ABI](../src/SetupBridge.h)。 |
| 修改部署和保存 | [SetupCommandLine.cpp](../src/SetupCommandLine.cpp) 解析后台请求。后台和图形界面共用 [SetupCore.cpp](../src/SetupCore.cpp) 的预检、写入和回滚逻辑。 |
| 修改下载、构建或打包流程 | [setup.ps1](../setup.ps1) 下载并调用配置器。[build.cmd](../build.cmd) 调用 [desktop.ps1](../frontend/desktop.ps1) 构建程序。[release.cmd](../release.cmd) 是生成本地发布附件的入口。 |
| 运行检查 | [test-build.cmd](../tests/test-build.cmd) 运行隔离回归，[analyze.cmd](../tests/analyze.cmd) 运行静态分析。单独核对文档链接和示例用 [documentation-tests.ps1](../tests/documentation-tests.ps1)。 |

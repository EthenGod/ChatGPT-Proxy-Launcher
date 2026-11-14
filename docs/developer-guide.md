# 开发者接入指南

适用于 v1.0.7 开发版。当前未发布。接入分两类：外部工具调用配置器部署；插件由启动器启动。两者都不需要常驻服务或新增运行库。

## 选择入口

| 要做什么 | 使用什么 |
| --- | --- |
| 在后台部署启动器 | 配置器 `--headless plan/deploy`；[请求示例](examples/deploy-v1.json) |
| 下载配置器后在后台部署 | `setup.ps1 -Headless -Request <绝对路径> -Result <新文件>` |
| 注册、配置、停用或移除插件 | 配置器 `--headless companion …`；[配置示例](examples/companion-config-v1.json) |
| 查询已部署启动器 | 配置器 `--headless info`；已知兼容启动器可用 `--launcher-info` |
| 取得一次启动的插件结果 | 启动器 `--launcher-chain --result <新文件> -- <ChatGPT 参数>` |
| 启动未适配规范的普通程序 | 配置器高级页的“添加程序”，沿用 `StartupApps` |

可复制的完整命令在[自动化用法](automation-usage.md)。部署先运行 `plan`，检查结果后再执行 `deploy`。等待进程退出再读结果；每次使用新的 JSON 文件名。

退出码 `0` 表示请求完成，`10` 表示有待办或部分失败。还要检查 `committed`、`warnings`、`actionsRequired` 或各扩展状态。`info` 成功只表示取得状态，是否部署要看 `data.state`。后台部署不会关闭或启动 ChatGPT。

## 接入一个插件

1. 在程序旁放 `companion.json`。使用稳定 ID，升级时不改 ID，也尽量不改清单路径。
2. 先用最小清单，不开启重试。下面适用于会持续运行的 EXE：

```json
{
  "schemaVersion": 1,
  "id": "org.example.addon",
  "name": "Example Addon",
  "version": "1.0.0",
  "requires": ["ready-file-v1"],
  "entry": {"runtime": "exe", "path": "Addon.exe", "arguments": []},
  "readiness": {"mode": "signal"}
}
```

3. 用 `companion register --manifest <清单绝对路径> --result <新文件>` 注册。它不会执行插件。实际启动发生在下一次由启动器创建 ChatGPT 进程后。
4. 入口检查 `CGL_CHAIN_PROTOCOL=1`，从 `CGL_CONTEXT_FILE` 读取本次上下文。一次性保存需要的参数和配置，不能依赖临时文件长期存在。没有协议环境时，由插件决定独立运行或说明用法，不伪造就绪回执。
5. 初始化真实功能。准备好后，将 `schemaVersion`、`runId`、`attemptId`、`token` 和 `status:"ready"` 写入 `reply.json.tmp`。关闭文件，再同目录改名到上下文给出的 `replyPath`。每次只写一份，不能覆盖。
6. 保持关联进程继续运行。启动器还检查 1 秒存活。短任务改用 `exit-code`，结束时返回成功退出码，不写回执。`timeoutMs` 包含初始化和这 1 秒，通常保留默认 15 秒。

PowerShell 可直接分发并点调用 [companion-context.ps1](examples/companion-context.ps1)，使用 `Read-CglContext`、`Send-CglReply`。其他语言只需文件、JSON 和环境变量接口，无需依赖本项目的 C++ 模块。字段见[上下文和回执规范](companion-protocol-v1.md)。

`already-running` 只能在确认已有实例真的可用后发送，须附带该进程的 PID 和创建时间。不能只按进程名猜测。启动器创建 ChatGPT 进程不表示 ChatGPT 已登录或可以联网；插件自行等待所需连接。

## 配置、失败与更新

- 作者清单与用户覆盖项分开。`arguments`、`config` 和整个 `overrides` 都是整体替换；只传 `enabled:false` 也会清除旧覆盖项。需要保留时由调用者提交完整覆盖项。
- 清单损坏、入口丢失时仍可停用。停用项不读取作者清单，列表版本为空；重新启用时重新校验。移除只删除指定 ID 的注册文件，保留程序与数据。注册目录超限时仍可停用或移除该项，并返回 `registry-read-incomplete` 警告。
- 默认只尝试一次。开启重试需要 `safe:true`、`tracked-retry-v1`、可恢复错误，以及上一轮整个进程族已经退出。不能使用 Job 跟踪的服务、WMI 或外部宿主代启动方式，不应声明安全重试。超时不会强杀进程。
- 扩展路径迁移时重新注册并加 `--replace`，保留用户覆盖项。新增参数优先放扩展自己的 `config`，不要随意扩充公共协议字段。
- 产品版本、JSON 格式版本、链协议版本各自独立。旧版拒绝未知字段；破坏兼容的字段或语义必须升级相应格式。可选新行为用新能力名，旧消费者先检查能力再使用。产品版本升高不代表某能力必然存在。
- `discovery.json` 是线索，`managedBy` 是归属标记，都不是签名。不要用新参数探测旧 EXE。本规范用于用户信任的本机扩展，不隔离同权限恶意程序。

## 修改本项目时看哪里

| 需要修改 | 位置与边界 |
| --- | --- |
| 协议版本、能力、限制 | `src/AutomationContract.h`；不和 `AppVersion.h` 混用 |
| 部署发现、版本判断 | `src/DeploymentDiscovery.*`；两个 EXE 共用，只读元数据 |
| 请求与部署 | `src/SetupCommandLine.cpp`；文件提交和回滚仍在 `SetupCore.cpp` |
| 清单、参数覆盖、依赖 | `src/CompanionManifest.*`；注册文件操作在 `CompanionRegistry.cpp` |
| 回执字段 | `src/CompanionProtocol.*`；完整校验后才交给运行逻辑 |
| 进程、等待与重试 | `src/CompanionRunner.cpp`；保持限时等待，不加常驻轮询 |
| 新脚本类型 | `src/StartupApps.*` 和清单运行方式映射；明确宿主，不猜文件内容 |

改接口时同步[Schema](schemas/)、[示例](examples/)和规范。结构校验不能替代文件归属、真实路径、进程身份、字节大小和依赖检查。`tests/contract-tests.cpp` 检查公共常量与 Schema、示例是否一致，不是完整 Schema 验证器。

在项目根目录运行 `cmd /c tests\test-build.cmd` 和 `cmd /c tests\analyze.cmd`。涉及等待路径时再运行 `work\chain-tests.exe "%CD%\work" --benchmark`（cmd）。测试只使用隔离目录和测试进程。新增文件同步构建、测试、分析及 `release.ps1` 的源码清单。

完整字段分别见[后台部署接口](automation-design.md)、[链式启动规范](companion-protocol-v1.md)。本轮发现的问题和验证范围见[复审记录](automation-maintenance.md)。

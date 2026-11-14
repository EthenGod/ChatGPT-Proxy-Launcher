# 项目协作约定

## 项目与命令
- 这是 Windows x64 原生 C++ ChatGPT 启动器，不是 OpenAI 官方产品。
- `src/ChatGPTProxyLauncher.cpp` 是正式源码；`README.md` 说明使用方法。
- 图形配置工具入口为 `src/ChatGPTLauncherSetup.cpp`，部署文件操作在 `src/SetupCore.cpp`；构建时内嵌同次生成的启动器。
- `src/SetupArguments.cpp` 只处理启动参数的生成、解析和校验；`src/CodexEnv.cpp` 只合并 `.env` 字节。文件读写、所有权检查与回滚统一留在 `src/SetupCore.cpp`。
- 构建：根目录运行 `cmd /c build.cmd`，需要 Visual Studio / Build Tools 的 C++ 桌面开发组件和 Windows SDK；产物在 `outputs/`。
- `tools/msvc-env.cmd` 是共用工具链入口，优先复用 x64 开发者终端，否则用 Visual Studio Installer 的 `vswhere` 定位带 C++ 工具的安装。
- `src/NativeMemory.h` 管理 Windows 返回的内存和 COM 引用；`tests/memory-tests.cpp` 用分配失败替身检查释放。线程和内存审查见 `docs/memory-thread-review.md`。
- 测试：根目录运行 `cmd /c tests\test-build.cmd`；只启动隔离测试子进程，不启动 ChatGPT。
- 静态分析：根目录运行 `cmd /c tests\analyze.cmd`。性能方案与复测方法见 `docs/performance-review.md`；`work\proxy-tests.exe --benchmark` 只解析合成配置，不读取本机代理。
- `setup.ps1` 是 PowerShell 下载入口；`tests/download-tests.ps1` 替换网络和启动操作，验证下载与校验。
- `tests/preview-download.ps1` 用本机限速数据预览真实下载进度条，不访问 GitHub 或启动配置器。
- 发布文件：根目录运行 `cmd /c release.cmd`，重新构建和测试后按部署器 `FileVersion` 生成 `Releases/v版本号/` 中的两个 EXE、对应源码 ZIP 与校验值，共四个上传附件；中文说明单独放在 `Releases/README.md`，不自动上传。两个 EXE 的版本统一维护在 `src/AppVersion.h`，升级时同步 `src/Setup.manifest` 和 `src/Launcher.manifest`。`release.ps1` 使用明确源码清单，新增源码/资源时须同步清单；`Releases/` 不提交。
- 正式测试和入口统一放在 `tests/` 并提交。测试先构建正式程序，文件操作放在 `work/` 的隔离目录。自启和 Windows 设置调用必须替换，不改真实用户配置。`work/` 也可能包含本机备份。
- `src/ProxyDetection.cpp` 负责只读端口发现；`tests/proxy-tests.cpp` 使用合成数据，界面测试替换真实检测，不依赖本机代理配置。
- `src/StartupApps.cpp` 处理 ChatGPT 启动后的附加程序。只读专用目录第一层；`tests/companion-tests.cpp` 用隔离目录和测试子进程验证。
- `src/SetupCommandLine.cpp` 是后台部署及扩展注册入口。`AutomationJson` / `AutomationIO` 提供有界 JSON、路径、结果文件和用户锁；GUI 与后台共用 `SetupCore::Preflight/Apply`。
- `src/AutomationContract.h` 集中公共协议版本、能力和限制；`DeploymentDiscovery.*` 统一两个 EXE 的只读发现和版本判断；`CompanionProtocol.*` 只解析回执，完整校验后才交给进程逻辑。
- `src/CompanionManifest.cpp` 校验清单和依赖；`CompanionRegistry.cpp` 保存注册及覆盖项；`CompanionRunner.cpp` 管理有限时长的就绪检查和重试；`LauncherAutomation.cpp` 提供发现查询、显式结果和限时提示。
- 开发入口见 `docs/developer-guide.md`，完整命令见 `docs/automation-usage.md`，接口见 `docs/automation-design.md`、`docs/companion-protocol-v1.md`；Schema、示例位于 `docs/schemas/`、`docs/examples/`。`tests/contract-tests.cpp` 检查常量与 Schema、示例一致性；隔离行为测试在 `tests/automation-tests.cpp`、`tests/chain-tests.cpp`、`tests/headless-download-tests.ps1`、`tests/protocol-example-tests.ps1`。性能复测为 `work\chain-tests.exe "%CD%\work" --benchmark`。
- `src/CodexClose.cpp` 在 `.env` 保存成功后请求整个 ChatGPT 桌面应用退出，并检查附属后台进程。只调用主窗口的应用退出入口，不向辅助窗口发送关闭消息。`tests/codex-close-tests.cpp` 替换进程、窗口、键盘输入、焦点和等待调用，不关闭真实应用。

## 工作方式
- 尽量用中文大白话解释。先读相关源码和文档，明确目标、影响文件及验收方法，再做小范围修改。
- 小改动在任务说明里写清计划即可；复杂改动在 `docs/` 留下简短的目标、方案、风险和验证记录，变更时同步更新。
- 保持原生、轻量、启动后退出；不新增常驻进程、自动日志、系统代理修改或强制结束 ChatGPT 的行为。
- 保留参数原样传递、显式程序路径和 `CreateProcessW` 调用；不经命令解释器转发用户参数，不硬编码个人代理或版本目录。
- 后台模式不得回退 GUI，不自动退出 ChatGPT，不打开设置、文件夹或网页。结果文件必须新建，调用者等待进程退出后读取。PowerShell 执行前检查 EXE 最低支持版本，不能用新参数探测旧版。
- 扩展重试须有安全声明、可恢复错误和上一轮完整进程族已退出的证据。不强杀、不设置 Job 的退出即终止标志。根进程句柄已触发不等于 Job 计数已归零。
- 修改程序行为后运行构建和相关测试；涉及参数传递时检查空参数、空格、引号、中文、长度边界和错误返回。
- 自绘操作按钮使用 `BS_OWNERDRAW` 和 `WM_DRAWITEM`，不能只在原生绘制后补刷；默认按钮样式切换须保留自绘，并验证 Tab、回车和空格行为，防止原生矩形闪现回归。
- 日常验收以源码审查、构建和相关测试为准；不要自行加入杀毒软件检查流程。
- 更新后运行中任务栏图标空白时，先检查窗口图标及相同 AppUserModelID 的开始菜单快捷方式是否指向已删除版本；修复快捷方式须备份并保留参数和应用标识，不直接清缓存或重启资源管理器。
- `src/TaskbarShortcut.cpp` 负责启动前的有限快捷方式修复；`tests/taskbar-tests.cpp` 只用隔离目录并替换 Shell 通知。检查模式必须跳过修复，修复失败不能阻止应用启动。
- 完成时说明改了什么、验证结果和未验证事项。编译通过或退出码为 0，不等于实际自启、静默启动或联网成功。
- 使用方法变了就更新 README；不要把某台机器的旧测试结果写成当前保证。重复出现的项目问题再补入本文件，保持简短。
- Release 发布说明尽量简洁，用大白话写用户能感受到的更新、修复和已知问题（若有）；少用专业术语，不堆砌代码整理、构建和测试细节。保留醒目的 `ChatGPTLauncherSetup.exe` 下载按钮和必要的使用提示。

## 隐私与操作边界
- 不提交密钥、令牌、密码、真实用户路径、私人代理配置、快捷方式、数据库、命令行捕获和本机验证记录；示例用占位符或回环地址。
- `--launcher-check` 会打印路径和完整参数，分享前必须脱敏。不要把敏感内容抄进文档、提交信息或回复。
- 提交前检查 `git status --short --untracked-files=all`、`git diff --check` 和实际待提交文件；用 `git check-ignore --no-index -v <路径>` 核对忽略规则。
- `.gitignore` 不会移除已跟踪文件或历史泄露；发现这种情况先说明范围，再处理。
- 未获授权不要安装、替换用户正在使用的程序、修改自启或发布；已获授权的操作不重复确认。
- 禁止批量删除文件或目录；禁止 `del /s`、`rd /s`、`rmdir /s`、`Remove-Item -Recurse`、`rm -rf`，也不要用脚本循环绕过。
- 需要删除时，一次只能删除一个明确路径的文件；需要批量清理时停止并请用户手动删除。

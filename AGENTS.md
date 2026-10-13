# 项目协作约定

## 项目与命令
- 这是 Windows x64 原生 C++ ChatGPT 启动器，不是 OpenAI 官方产品。
- `src/ChatGPTProxyLauncher.cpp` 是正式源码；`README.md` 说明使用方法。
- 图形配置工具入口为 `src/ChatGPTLauncherSetup.cpp`，部署与参数逻辑在 `src/SetupCore.cpp`；构建时内嵌同次生成的启动器。
- 构建：根目录运行 `cmd /c build.cmd`，需要 VS 2022 Build Tools 的 C++ 工具；产物在 `outputs/`。
- 测试：根目录运行 `cmd /c work\test-build.cmd`；只启动隔离测试子进程，不启动 ChatGPT。
- 发布文件：根目录运行 `cmd /c release.cmd`，重新构建和测试后按部署器 `FileVersion` 生成 `Releases/v版本号/` 中的两个 EXE、对应源码 ZIP 与校验值，共四个上传附件；中文说明单独放在 `Releases/README.md`，不自动上传。版本维护在 `src/SetupResources.rc`。`release.ps1` 使用明确源码清单，新增源码/资源时须同步清单；`Releases/` 不提交。
- `work/launcher-tests.cpp`、`work/test-build.cmd` 需要提交；`work/` 其余内容和 `outputs/` 都是本机文件。
- `tests/setup-tests.cpp` 需要提交；测试脚本先构建正式程序，部署测试仅使用 `work/` 下的隔离目录，不修改真实自启项。
- `tests/startup-tests.cpp`、`tests/setup-ui-tests.cpp` 也需要提交；系统自启和 Windows 设置调用必须在测试中替换，不能用真实用户配置测试。
- `src/ProxyDetection.cpp` 负责只读端口发现；`tests/proxy-tests.cpp` 使用合成数据，界面测试替换真实检测，不依赖本机代理配置。

## 工作方式
- 尽量用中文大白话解释。先读相关源码和文档，明确目标、影响文件及验收方法，再做小范围修改。
- 小改动在任务说明里写清计划即可；复杂改动在 `docs/` 留下简短的目标、方案、风险和验证记录，变更时同步更新。
- 保持原生、轻量、启动后退出；不新增常驻进程、自动日志、系统代理修改或强制结束 ChatGPT 的行为。
- 保留参数原样传递、显式程序路径和 `CreateProcessW` 调用；不经命令解释器转发用户参数，不硬编码个人代理或版本目录。
- 修改程序行为后运行构建和相关测试；涉及参数传递时检查空参数、空格、引号、中文、长度边界和错误返回。
- 自绘操作按钮使用 `BS_OWNERDRAW` 和 `WM_DRAWITEM`，不能只在原生绘制后补刷；默认按钮样式切换须保留自绘，并验证 Tab、回车和空格行为，防止原生矩形闪现回归。
- 日常验收以源码审查、构建和相关测试为准；不要自行加入杀毒软件检查流程。
- 完成时说明改了什么、验证结果和未验证事项。编译通过或退出码为 0，不等于实际自启、静默启动或联网成功。
- 使用方法变了就更新 README；不要把某台机器的旧测试结果写成当前保证。重复出现的项目问题再补入本文件，保持简短。

## 隐私与操作边界
- 不提交密钥、令牌、密码、真实用户路径、私人代理配置、快捷方式、数据库、命令行捕获和本机验证记录；示例用占位符或回环地址。
- `--launcher-check` 会打印路径和完整参数，分享前必须脱敏。不要把敏感内容抄进文档、提交信息或回复。
- 提交前检查 `git status --short --untracked-files=all`、`git diff --check` 和实际待提交文件；用 `git check-ignore --no-index -v <路径>` 核对忽略规则。
- `.gitignore` 不会移除已跟踪文件或历史泄露；发现这种情况先说明范围，再处理。
- 未获授权不要安装、替换用户正在使用的程序、修改自启或发布；已获授权的操作不重复确认。
- 禁止批量删除文件或目录；禁止 `del /s`、`rd /s`、`rmdir /s`、`Remove-Item -Recurse`、`rm -rf`，也不要用脚本循环绕过。
- 需要删除时，一次只能删除一个明确路径的文件；需要批量清理时停止并请用户手动删除。

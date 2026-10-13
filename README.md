# ChatGPT 轻量启动器

本地制作的 Windows x64 原生程序，并非 OpenAI 官方产品。只负责查询当前用户注册的 ChatGPT 安装路径、传递命令行参数并启动，然后立即退出。无需加载 .NET，不等待 Clash，不常驻，不显示检查窗口，不写日志，不改变系统代理。

## 许可证

本项目采用 GNU 通用公共许可证第 3 版（仅此版本，`GPL-3.0-only`），完整条款见 [LICENSE](LICENSE)。此授权适用于本项目自身的代码，不改变 ChatGPT 或其他第三方软件的许可。ChatGPT 图标的来源和权利说明见 [assets/README.md](assets/README.md)。

## 图形部署工具（推荐）

从 Release 下载 `ChatGPTLauncherSetup.exe`，双击打开。它已经包含同次构建的启动器，不用额外下载或手动复制文件。

部署器、启动器 EXE 和生成的快捷方式均使用内嵌的 ChatGPT 图标，无需另外保存图标文件。旧版本用户重新运行新版部署器并保存后，部署文件和快捷方式会更新；Windows 图标缓存可能稍后才刷新。

1. 首次打开时，“登录 Windows 后自动启动”和“启动时不显示主窗口”默认勾选；已经保存过配置时，会恢复之前的选择，不会强行重新勾选。
2. 需要代理时，勾选“使用指定代理”，选择协议和地址，再点击空白端口框（或用 Tab 移入），工具会尝试自动检测并填入。已有端口不会被覆盖；需要重新检测时，清空后再次点击。检测失败或有多个结果时，下方会提示去代理工具的“端口设置”查看并手动填写。地址只填主机名或 IP，IPv6 使用 `[::1]` 这样的格式。
3. **启动参数预览**会随选项和输入实时更新，可以选中复制。它展示的是快捷方式的参数部分；空白表示不附加参数。输入无效时显示原因，并暂时禁止保存。
4. 点击左下角低调的 **高级 ›**，进入二级设置页。这里可以修改部署目录（直接输入、浏览文件夹或恢复默认），也可填写代理绕过列表和额外参数；点击 **‹ 返回** 或按 Esc 返回主界面，输入会保留。两页都有实时参数预览。
5. 点击“部署并保存”。成功后保存按钮显示“已保存”，点击“完成”即可关闭；没有更改时无需再次部署。以后重新打开工具，修改后点击“保存更改”。

默认将启动器部署到 `%LOCALAPPDATA%\ChatGPTProxyLauncher\ChatGPTProxyLauncher.exe`，勾选登录自启时，在当前用户的启动文件夹创建 `ChatGPT Proxy Launcher.lnk`。自定义目录支持本机磁盘的绝对路径及中文、空格，不支持网络共享路径；过长或无效路径会提示。无权写入的目录需要换一个位置，工具不会自动提权。

**端口检测范围**：本机地址 `127.0.0.1` / `localhost`，HTTP 或 SOCKS5 协议。代理工具需要正在运行，配置中的端口还必须与对应内核正在监听的 IPv4 端口一致。

| 工具或来源 | 支持的常见情况 |
| --- | --- |
| Clash Verge Rev、FlClash | 常见用户配置目录中的运行配置，优先混合端口，其次为对应协议端口 |
| CFW（Clash for Windows） | 用户目录 `.config/clash/config.yaml`；运行中 CFW 程序旁的便携 `data/config.yaml` |
| v2rayN | 程序目录及 `%LOCALAPPDATA%/v2rayN` 中的 `binConfigs/config.json`；旧版程序旁的 `config.json`。支持 Xray、V2Ray、sing-box 的本地 HTTP/SOCKS 入站及混合入站 |
| Windows 系统代理 | 已启用且有本地监听的 HTTP 代理，适用于其他写入系统代理设置的工具 |

v2rayN 有独立 HTTP 入站时优先使用它；Xray 的 SOCKS 入站按其现行混合协议行为识别，V2Ray 的 SOCKS 入站不当作 HTTP。不读取远程节点、控制接口或 TUN 端口作为代理端口；需要认证或自定义传输的 JSON 入站会跳过。远程地址、IPv6、HTTPS 代理、特殊安装/配置方式或不能确定的情况需要手填；混合端口通常应选 HTTP 或 SOCKS5，HTTPS 代理不是“访问 HTTPS 网站”的意思。检测期间可继续输入，过时的结果会丢弃。检测只读配置和本机状态，不改变系统代理、不请求外部网站、不启动代理软件；填入成功不代表联网已成功。

**勾选登录自启并保存成功后**，工具会尝试关闭能够准确识别的 ChatGPT 传统自启项，并显示“部署已完成”。关闭提示后会打开 Windows 的“启动应用”设置供核对。同一次打开工具期间，这项尝试和提醒只执行一次；之后修改其他选项并保存不会再次提醒，重新打开工具则重新计算。

对于传统自启，工具只处理当前用户 `Run` 中目标路径与已注册应用包内 `app\ChatGPT.exe` 一致的条目，保留原条目内容，仅设置禁用标记。对于商店自启，先验证应用包身份，从清单读取指向该程序的 `StartupTask` 标识，再检查当前用户已有的任务状态：已经关闭就不改动；普通启用状态则尝试设为“用户已禁用”，并回读确认。不会凭显示名称关闭其他程序，不新建未知任务项，不修改策略控制状态或其他注册表值。写入前保留原值，回读失败时尝试恢复并报告恢复结果。

商店自启采用 Windows 现有 MSIX 状态记录的兼容处理，不是 ChatGPT 提供的接口；遇到缺失、未知状态、权限或版本差异时会提示手动核对。清单中的 `Enabled` 声明不作为用户当前状态。请在 Windows 设置中确认 ChatGPT 自带自启已关闭，并保留本工具的启动项；如果你已经关闭，保持关闭即可，不需要回到工具重新部署。检查提醒不影响已保存的配置；即使设置页面打不开，也可点击“完成”，之后手动进入“设置 → 应用 → 启动”核对。单纯勾选但未保存、或保存失败时，不执行这些系统操作。

取消勾选登录自启并保存，只移除本工具管理的那一个自启快捷方式。程序和配置保留，不会批量删除文件。修改部署目录会将启动器部署到新目录并更新快捷方式，旧目录中的文件保留，用户可以按需手动处理。

配置记录保存在 `%LOCALAPPDATA%\ChatGPTProxyLauncher\Launcher settings.lnk`，与实际部署目录分开，因此关闭自启后仍可恢复设置。启动文件夹中的受管快捷方式若被手动修改，重新打开工具时会优先读取其参数，未知选项保留在“额外启动参数”中。不要把同一选项同时填在额外参数和对应控件中；`--launcher-check` 不能保存为启动选项。配置工具的参数上限为 8191 个 UTF-16 字符，并在保存前回读快捷方式核对内容。

如果目标目录中有来源不明的同名 EXE，或启动文件夹中有其他同名快捷方式，工具会停止而不是直接覆盖。关闭冲突错误提示后，会自动打开冲突文件所在的文件夹，方便查看；配置记录被其他快捷方式占用时也会打开对应目录。如果文件夹无法打开，提示会给出路径，供手动查看。同名 EXE 冲突可通过选择其他部署目录解决；以前手动创建的同名自启快捷方式也不会自动接管，需要先自行改名或移走。成功保存并检查自启设置后可以关闭配置工具；它不会自动启动 ChatGPT，也不会修改系统代理。取消本工具的登录自启不会替你重新启用应用自带自启。

建议关闭 ChatGPT 自身的登录自启，避免另一个入口先启动。保存成功只表示文件和快捷方式已写入并通过检查，实际自启、静默效果及联网仍需使用时验证。

## 手动传入参数

代理和静默启动都通过参数设置，EXE 内没有预设的代理地址：

```cmd
ChatGPTProxyLauncher.exe --proxy-server=http://127.0.0.1:7897 --no-startup-window
```

正常显示窗口时去掉 `--no-startup-window`。双击没有参数的 EXE，只会按 ChatGPT 自身的默认行为启动。其他参数（如 `--proxy-bypass-list="localhost;*.example.com"`）也会原样传给 ChatGPT，是否生效由 ChatGPT 决定。空格、引号、中文及参数顺序均保留，不经过命令解释器。

唯一保留给启动器的选项是放在最前面的 `--launcher-check`：只向标准输出打印查到的路径和待传参数，不启动 ChatGPT。例如在命令提示符中：

```cmd
ChatGPTProxyLauncher.exe --launcher-check --proxy-server=http://127.0.0.1:7897 --no-startup-window > check.txt
```

失败时返回 Windows 错误码，错误信息写到标准错误输出，不弹窗或生成日志。退出码 0 表示已提交启动请求（检查模式则表示检查通过），不代表 ChatGPT 初始化或联网成功。

如果 ChatGPT 已经运行，新代理参数通常不能改变现有实例；应在下次完整退出后启动时使用。启动器不会结束现有 ChatGPT 进程。

## 自动适配更新

使用 Windows 包接口查询 `OpenAI.Codex_2p2nqsd0c76g0`，按实际版本号选最高的主应用包，拼接 `app\ChatGPT.exe`。不固定版本目录，也不扫描整个磁盘。查询期间包列表变化或安装路径消失时，最多尝试 3 次，不等待；权限等其他错误立即返回。最高版本程序仍缺失时停止，不会回退到旧版本。未来若包身份或内部目录改变，仍需调整源码。

## 编译与体积

启动器源码为 `src\ChatGPTProxyLauncher.cpp`；配置界面为 `src\ChatGPTLauncherSetup.cpp`，部署和参数处理为 `src\SetupCore.cpp`，自启尝试与 Windows 设置入口在 `src\StartupIntegration.cpp`。在项目根目录运行 `build.cmd`，需要 Visual Studio 2022 Build Tools 的 C++ 工具和 Windows SDK。

构建顺序为：编译启动器 → 把该 EXE 编入配置工具资源 → 编译配置工具。产物是 `outputs\ChatGPTProxyLauncher.exe` 和 `outputs\ChatGPTLauncherSetup.exe`。配置工具采用 Win32 / GDI+，统一绘制开关、协议分段选择、按钮、圆角输入框和滚动条，保留系统文本输入与键盘行为。它支持系统浅深色、高对比度及 DPI 缩放；较小的窗口可滚动，支持滚轮、拖动滚动条和 Page Up / Page Down。它也静态链接运行库，不需要额外安装 .NET 或 WebView2。

两个程序均静态链接运行库，保留编译器默认安全检查和 ASLR/DEP，且内嵌多尺寸 ChatGPT 图标；体积以实际构建文件为准。原启动器只导入系统 `KERNEL32.dll`；配置工具另使用 Windows 的界面、文件选择、JSON 解析和快捷方式等系统组件，不需要另装运行库。

命令行和工作目录按实际长度分配内存，缓冲区直接由 API 或复制操作填充，避免重复清零。检查模式只校验命令行长度，不构造未使用的命令行；启动成功直接返回，不生成错误提示。退出后不保留进程。

`work\test-build.cmd` 可编译并运行隔离测试，覆盖数字版本排序、资源包和其他身份排除、有限重查、内存分配失败、错误返回、工作目录、命令行长度边界以及实际子进程参数传递。测试用替换接口只存在于测试程序中，不编入正式 EXE，不启动 ChatGPT。复查方案与验证范围见 [docs/launcher-review.md](docs/launcher-review.md)。

`work\test-build.cmd` 会先重新构建正式程序，再运行五组测试，避免误测旧产物。其中 `tests\setup-tests.cpp` 在 `work\setup-tests-*` 的隔离目录验证部署、自启快捷方式读写、目录迁移、未知文件保护、失败回滚、参数往返及内嵌资源一致性。`tests\startup-tests.cpp` 使用模拟注册表验证目标筛选与失败返回；`tests\proxy-tests.cpp` 使用合成配置和进程信息检查代理端口识别；`tests\setup-ui-tests.cpp` 使用独立测试窗口，验证默认选项、页面切换、实时预览、滚动和保存流程，并在 `work/` 生成控件渲染图。测试替换了修改自启、打开系统设置和提醒窗口的接口，不写真实自启项。测试产生的本机文件会保留，需清理时请手动处理。配置工具方案及验证边界见 [docs/setup-design.md](docs/setup-design.md)。

在项目根目录运行 `release.cmd`，会先重新构建并跑完五组测试，再按部署器 EXE 的 `FileVersion` 创建版本目录，例如 `Releases/v1.0.0/`。版本信息维护在 `src/SetupResources.rc` 中，更新时保持数字版本和字符串版本一致。准备上传的文件为：

| 文件 | 用途 |
| --- | --- |
| `ChatGPTLauncherSetup.exe` | 推荐普通用户下载，已包含启动器 |
| `ChatGPTProxyLauncher.exe` | 供手动配置用户单独下载 |
| `ChatGPTLauncher-source.zip` | 与本次构建对应的源码、资源和构建测试脚本 |
| `SHA256SUMS.txt` | 上述三个下载文件的 SHA-256 校验值 |

将对应版本文件夹中的这四个文件作为 GitHub Release 附件上传即可。简短中文说明放在 `Releases/README.md`，无需上传；说明源文件为 [docs/release-upload.md](docs/release-upload.md)。不再生成内容重复的整合 ZIP。不同版本分开保存；重新打包同一版本会更新该版本的文件。打包脚本不会发布或上传到 GitHub。源码包使用明确的文件清单，不包含 `.git`、本机配置、临时测试数据或截图；发布时仍应提交并标记对应源码版本。脚本核对源码在构建打包期间未变、ZIP 内每个文件的内容及复制后的校验值。`Releases/` 和 `outputs/` 均由 Git 忽略，不把编译产物提交到源码仓库。暂存文件保留在 `work/`，需要清理时由用户手动处理。

## 开机启动

建议安装位置：`%LOCALAPPDATA%\ChatGPTProxyLauncher\ChatGPTProxyLauncher.exe`。

在启动文件夹创建 `ChatGPT Proxy Launcher.lnk`，直接指向这个 EXE。以下是参数示例，端口请按自己的代理软件设置：

```text
--proxy-server=http://127.0.0.1:7897 --no-startup-window
```

以后更换代理端口，只需在快捷方式属性的“目标”末尾修改参数，不用重新编译。使用此方式时，建议关闭 ChatGPT 官方自启，避免另一个入口抢先启动。停止自启可在 `Win+R` 输入 `shell:startup`，删除这个快捷方式。

`--proxy-server` 不保证语音及所有子工具流量都受它控制；登录自启、静默效果和联网需要在实际使用中验证。

## 开发与提交

协作规则见 [AGENTS.md](AGENTS.md)。源码、资源、构建脚本、`tests/` 和两个 `work/` 测试文件纳入版本管理；构建产物、备份与本机记录由 `.gitignore` 排除。

`--launcher-check` 输出包含实际安装路径和完整参数；分享前应去掉用户名、私人地址和凭据。示例中的 `127.0.0.1` 是本机回环地址，包名称和发布者 ID 是应用标识，不是账号凭据。

接口依据：[Microsoft CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)、[Microsoft GetPackagesByPackageFamily](https://learn.microsoft.com/en-us/windows/win32/api/appmodel/nf-appmodel-getpackagesbypackagefamily)。

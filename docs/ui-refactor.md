# 配置器界面重构

默认配置器使用 React、TypeScript 和 Tauri 2。Rust 通过 C ABI 调用原有 C++ 部署核心。单个 EXE 包含正式界面、原生兼容界面和同次构建的启动器。后台命令在创建窗口前分流。

## 当前验收状态

本轮复核覆盖并发构建、偏好写入失败、页面重载、附加程序回退和下载清理。测试记录通过替换文件发布，WebView2 使用本次新建的目录。生命周期检查用 Windows Job 统计窗口及其后代。相关检查均使用隔离路径。

以下清单区分隔离回归与实机检查。后文保留交付基线和复测方法。

| 项目 | 状态与边界 |
| --- | --- |
| 构建、静态分析、自动回归、单文件与源码包 | 已通过。源码包和正式 EXE 均有独立交付检查。 |
| 隔离部署、恢复、回滚、并发、快捷方式、附加程序、原生文件选择与命令权限 | 已通过。文件操作只在隔离目录，系统副作用使用替身。 |
| 重复打开配置器、兼容窗口图标和启动失败关闭 | 隔离桌面验证了隐藏或最小化窗口的恢复、兼容窗口的大小图标和禁用页面脚本时的关闭操作。原生测试模拟了另一会话占用配置器，真实双登录会话待验。 |
| 三语、主题、语言动画、键盘、短窗口、错误详情、媒体模拟和重复开关 | 已通过限定场景。媒体模拟不代表真实 Windows 设置。最终视觉效果仍由用户确认。 |
| 正式包读取已有配置与后台只读预检 | 已通过。`info` 和 `plan` 返回成功，预检不提交。原有部署、`.env`、快捷方式和已记录的偏好值不变。 |
| 真实端口发现与代理 HTTPS 连通 | 已通过。来源提示与详情可用；经当前代理访问两个公开 HTTPS 页面均返回 200，保留证书校验。未验证登录后的 Codex 对话或 WebSocket。 |
| Windows 150% / 200% 缩放、系统高对比度 | 用户明确要求暂缓，不操作这些设置。 |
| 跨显示器、独立机器与真实屏幕阅读器 | 未完成。当前没有第二显示器或可用的独立 Windows 测试环境；无障碍树检查不能代替朗读验收。 |
| WebView2 全新安装 | 未完成。真实引导程序下载和微软签名校验通过，未运行安装器。现有运行库已能打开正式界面；缺少运行库时的分流只用替身验证。 |
| 登录自启、实际退出 ChatGPT | 未完成真实操作。需要登录周期或独立观察；退出流程会影响承载本任务的桌面应用。已有隔离检查不替代这两项。 |

在 464 × 361 的 WebView2 中，中文保存提醒和操作按钮可并排显示，底栏从 99px 减到 57px。长帮助滚到底后，标题和按钮仍在原位，按 Esc 回到原问号。1184px 宽的窗口中，设置卡片居中，正文宽度为 744px。三种语言在深浅主题下共采样 180 帧。切换时文字有淡入淡出，工具栏、卡片、输入框和保存按钮的底色采样点均保持原色。

输入等待原生校验时保留上次已确认的参数，持续等待时显示校验提示。保存使用当前输入的校验结果。

两种界面共用同一把窗口锁。重复打开会唤醒原窗口，后台命令仍可单独运行。主窗口创建后即可关闭，页面接好关闭事件后才加载表单。保存成功会回读已部署的参数、版本和提示。回读失败会提醒用户重新打开，仍保留本次保存结果。

高级设置展开时单独读取附加程序。列表合并普通入口和注册扩展，适配标记来自清单校验。普通目录与注册目录独立读取，一侧不可读时仍显示另一侧。添加、手动刷新和返回窗口会更新列表。空闲时没有文件监听或轮询。

附加程序的操作集中在标题行。“已适配”紧跟名称。空间不足时名称显示省略号，停用状态和箭头仍在右侧。点击程序或按回车可查看完整名称、路径和启动参数。Esc 返回原程序行。

快捷方式优先显示指定图标，其次使用目标程序自带的图标。找不到时使用 Windows 文件类型图标。系统也未提供图标时，界面按扩展名绘制。列表最高 228px，能看到第六行的一部分。滚轮、键盘和拖动都能操作内层滚动条。滑块向左滑入并淡显，行尾状态和箭头同步让出 12px。停下后，滑块向右滑出并淡隐，行尾同步回到原位。

首次读取完成后，设置和程序行淡入。插入程序时，新行淡入，受影响的行平移。相同内容再次刷新时保持静止。主动添加的程序超出可视区时，只滚动内层列表。切换语言时，文字和边框的过渡都受所在视口裁剪。调整窗口大小时，布局直接跟随窗口。深浅主题同步设置原生窗口和 WebView 底色。

需要体验示例数据时，运行 `frontend/desktop.ps1 -Command sample`，然后双击 `work/ui-sample/ChatGPTLauncherSetup-Sample.exe`。示例版内嵌页面和数据，直接打开附加程序。右上方可切换常规、空列表、部分不可读及读取失败四种场景。后两种场景点刷新可恢复。此版本需要系统 WebView2。用 `tests/ui-prototype-tests.ps1 -Sample -Action Start -RemoteDebugging` 启动隔离窗口，再用 Playwright CLI 运行 `tests/ui-program-sample-checks.js` 检查交互。另开示例窗口运行 `tests/ui-program-clip-checks.js`，逐帧检查列表上、下边缘是否出现隐藏文字。

隔离桌面窗口已核对普通程序、已适配、已停用及注册异常四种显示。外部添加文件并停用扩展后，点刷新即可看到两项变化。整个过程只读两次列表，未触发保存。三语、深浅主题及用 Tab 查看程序路径均已通过。

图标验收使用带内嵌图标的 EXE、指定透明图标的快捷方式和脚本。移走快捷方式的图标文件后，刷新会显示目标文件类型图标。示例窗口在三种语言下检查了 546 和 268 像素宽的列表，也检查了滚轮、键盘、拖动和减少动画设置。

首次读取和重试时显示设置骨架，第一次校验参数时预览区也有占位。骨架与实际启动设置卡片同高同位。问号弹窗的标题与正文间距为 17px。隔离 WebView2 中分别检查了约 12ms 和 1 秒的端口检测。快速返回时直接显示对勾，较慢时先转圈。两种情况下图标位置均未移动。

问号说明由界面排段，段距为 8px。普通修改用保存按钮上的圆点表示，底栏保留保存结果、退出提醒和需要处理的问题。“查看输入问题”可直接定位到参数预览。输入改正后，“查看详情”立即停止响应，再淡出。隔离 WebView2 中检查了错误码 87 和 206，入口出现和收起时参数框均无位移。检查了协议、端口、问号、高级设置、主题、保存、语言菜单和临时提示的悬停。鼠标在端口输入框与检测按钮之间往返时，140 帧采样中的外框颜色一致。

第 4 阶段仅调整构建、交付边界和相应文档。原生编译抽成共用入口，默认配置器写入 `outputs/`，旧 GUI 单独保存在 `work/ui-native/`。正式前端不内嵌模拟页面，普通 EXE 不包含测试路径入口。风险主要是递归构建、混入旧资源或测试 feature、源码漏打包，以及 WebView2 的启动和内存开销。验收包含原有回归、桥接对照、TypeScript、Rust、静态分析、资源与源码包核验、60 秒空闲和重复关闭。测试只用隔离目录和系统操作替身；真实安装、自启、联网及退出 ChatGPT 另列待验。

第 3 阶段复用现有 C++ 参数、部署、回滚和系统操作。Rust 串行执行原生命令，任务结束前阻止关闭。文件选择沿用 Windows 对话框，保留 `.lnk` 本身；偏好沿用原注册表位置。桥接另传不依赖 JSON 分配的提交状态，避免保存成功后响应失败被误报为未写入。测试包只写隔离路径，代理检测、自启处理、应用退出和打开位置使用替身。实际桌面验收同时检查命令权限、原生选择器和进程退出。

第 2 阶段采用单页设置布局，共用按钮、开关、菜单、折叠区和弹窗。原型与桌面接口分开，业务交互统一通过有类型的接口。预览参数校验仅用于模拟交互，不能替代第 3 阶段的 C++ 校验。验收覆盖状态变化、检测竞争、关闭保护、键盘焦点、三语、短窗口、深浅色和无障碍设置。

## 分层与边界

- `frontend/src/features/setup/` 包含设置页、服务接口和参数校验状态。`components/` 提供 Base UI 控件，`styles/setup.css` 提供设置页样式，`i18n/` 读取根目录三语词条。模拟参数只由 `platform/mock.ts` 生成。
- `frontend/src/platform/setup-service.ts` 调用串行原生命令。`platform/mock.ts` 提供示例数据和内存选择器。正式页 `main.tsx` 和原型 `prototype.tsx` 共用 `platform/desktop.ts` 处理窗口就绪、主题和关闭事件。正式页添加程序时，由原生选择器取得文件路径。
- `features/setup/useSetupValidation.ts` 在输入停止后校验参数。成功结果可跨语言复用，错误结果按当前语言重取。只有当前输入通过校验才能保存。保存、目录选择、添加程序和关闭共用一处操作状态。关闭失败后恢复编辑。对应回归在 `tests/frontend/interaction.test.tsx` 和 `tests/frontend/desktop-service.test.ts`。
- `frontend/src-tauri/` 包含窗口、原生命令、Rust 数据类型和 C ABI 包装。主窗口可加载、校验、检测、保存、选择目录、列出和添加附加程序、打开指定位置及更改偏好。窗口权限限于显示、关闭、必要事件，以及设置自身主题和窗口、WebView 底色。
- `src/SetupBridge.*` 把现有核心接入有界 UTF-8 JSON。C++ 分配结果，Rust 作用域对象调用 C++ 释放。异常不跨越 ABI。保存另传独立提交状态，响应构造失败仍保留已提交结果。`src/SetupUiServices.*` 共用原生偏好、选择器和保存后处理。
- 现有部署核心与启动器不迁移。后台命令在 WebView2 检查和 Tauri 初始化之前分流。原始 UTF-16 参数不重新编码或拼接。
- 新 EXE 静态链接 C++ 核心、原生兼容界面和 Visual C++ 运行库。Tauri 生成图标和版本资源；额外资源放同次构建的启动器和界面能力信息，避免重复 manifest 或版本资源。
- `test-harness` 仅用于独立测试产物。它要求显式测试目录，隔离全部快捷方式、部署文件、`.env` 和 WebView 数据。测试观测文件只存在于该构建。普通产物不读取测试环境变量。
- `ui-prototype` 强制依赖 `test-harness`，不能用于正式交付。窗口关闭请求先交给页面：保存期间忽略，未保存时确认，确认后才由受限命令关闭。原型的目录与程序选择使用样例列表；功能包使用原生文件选择器。

## 构建与检查

### 原生兼容模式

- 同一个 EXE 支持 `--compatibility`。必须单独使用，不能混入后台参数。入口在 WebView2 检查和 Tauri 初始化前分流。
- 旧界面增加兼容模式标题与版本标识，三语使用相同布局测量。保存、回滚、代理、附加程序、主题和语言偏好继续复用原实现。
- PowerShell 在校验 EXE 后只读加载资源 102。没有该资源的旧配置器直接打开，不安装 WebView2，也不传入新参数。
- 新配置器按微软文档读取用户和机器的运行库版本。缺少运行库时显示安装（推荐）、兼容界面和取消。直接双击 EXE 则自动回退兼容界面。
- 选择安装后才下载微软 Evergreen Bootstrapper。执行前校验 Authenticode 状态和微软发布者，执行 `/silent /install`，结束后重新检查运行库。下载、签名、安装或检测失败时停止，不误报新界面可用。
- 取消不打开配置器或项目网页，清理本次下载后返回 PowerShell。后台命令不检查运行库，也不询问安装。
- 风险是下载安装依赖网络与本机策略。保留不联网的兼容入口。原生模式不创建 WebView 子进程，沿用原生界面的内存和线程生命周期。
- `tests/ui-stage1-tests.ps1` 用同一测试目录交叉打开两种界面，检查第二个进程退出、原窗口从隐藏或最小化恢复，以及兼容窗口的大小图标。隔离用例检查目录联接、部署位置和后台结果路径。测试还会暂停一次界面保存，同时发起后台部署，验证同一目录的别名共用操作锁。测试卷支持区分大小写时，再检查相邻目录能分别保存，快捷方式自定义图标和 EXE 图标都只能从各自根目录读取。`tests/setup-ui-tests.cpp` 模拟其他会话持有用户锁。

### 开发命令

需要 Node.js（24.x 从 24.15 起，或 26 及以上版本）、Rust 1.99 以上的 MSVC 工具链、Clippy、rustfmt、Visual Studio C++ 工具和 Windows SDK。前端依赖使用锁文件。缓存和产物放在 `work/`，不改系统 PATH。

先在 `frontend` 运行 `npm ci` 安装锁定的依赖。回到仓库根目录，按顺序运行静态分析和完整测试。

```powershell
cmd /c tests\analyze.cmd
cmd /c tests\test-build.cmd
```

若 Cargo 不在 PATH，可用 `-RustBin` 指定现有 MSVC 工具链的 `bin` 目录，或设置进程变量 `CHATGPT_RUST_BIN`。脚本结束后恢复环境变量。

根目录 `cmd /c build.cmd` 生成配套的正式配置器和启动器。`cmd /c build.cmd native` 在 `work/ui-native/` 编译旧 GUI 和内嵌的启动器。`cmd /c build.cmd launcher` 单独重建 `outputs/` 中的启动器。三个入口共用一把构建锁。旧 GUI 和独立启动器只需 C++ 工具和 Windows SDK。原型、功能测试版和示例版默认从 `work/ui-test-native/` 嵌入各自构建的启动器。如果最后一次只构建了正式启动器，交付前须再做一次完整构建，确保两个 EXE 配对。`tests/build-lock-tests.ps1` 检查编译和内嵌时是否持有同一把锁。

静态分析覆盖 C++ 和 Rust 的正式及测试配置。完整测试构建正式 EXE 和隔离配置器，检查 C++、C ABI、前端、Rust、后台命令和交付文件。在有桌面会话的 Windows 上，使用完整测试生成的隔离配置器继续检查实际窗口。

```powershell
.\tests\ui-stage1-tests.ps1
.\tests\ui-prototype-tests.ps1 -Functional
```

`cmd /c release.cmd -TestPackage` 运行完整检查后，只在新的 `work/release-stage-*` 目录生成两个 EXE、源码 ZIP 和校验文件，不改旧 Release 或已有部署。`tests/delivery-tests.ps1 -PackageDirectory <目录>` 可独立复核包内容。正式发布仍使用 `release.cmd`，本地打包不会上传。

在根目录构建并打开内存模拟预览。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\frontend\desktop.ps1 -Command prototype
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\frontend\preview.ps1
```

已有测试包时只运行第二条。在 `frontend` 目录中，把脚本路径改成 `.\preview.ps1`。`-ExecutionPolicy Bypass` 只作用于这次 PowerShell 进程，不修改系统或用户的执行策略。

预览脚本创建独立会话目录，再启动 `work/ui-prototype/ChatGPTLauncherSetup.exe`。不要单独双击该测试 EXE。所有设置、主题和语言更改仅保存在当前原型内存中。顶部状态菜单可切换首次部署、缺失或未知版本、保留的静默值、长路径、加载失败、保存过程、不同失败结果和保存后续操作。切换状态会重置本次演示输入。静默选项继续禁用。

浏览器预览可在 `frontend` 运行 `npm run dev`，访问 `http://127.0.0.1:5174/prototype.html`。它只验收页面交互，不能替代桌面壳验证。交互回归在 `tests/frontend/interaction.test.tsx`；桌面生命周期检查在根目录运行：

```powershell
.\tests\ui-prototype-tests.ps1
```

该检查默认观察 60 秒，并正常开关窗口三次。关闭测试使用与标题栏相同的系统命令，并核对事件计数。`-Action Start -RemoteDebugging` 只对本次隔离测试进程启用本地调试端口。普通预览不启用此端口。测试进程和子进程按 PID、启动时间与程序路径核对，不能关闭其他 WebView 进程。

默认配置器位于 `outputs/ChatGPTLauncherSetup.exe`，读取真实用户设置。隔离功能包位于 `work/ui-stage1-test/`，通过 `preview.ps1 -Functional` 打开后只写本次测试目录。测试源码位于 `tests/`；自动验收不保存真实用户配置。

桌面构建每次使用新的 `work/ui-assets/` 子目录。不会混入旧资源，也不自动删除旧文件。`work/ui-build.json` 与 `work/ui-stage1-test-build.json` 记录该次 EXE 和内嵌启动器的哈希。不要把这些本机记录加入源码包。

界面测试需要正常桌面会话。受限沙箱中的 WebView2 可能无法初始化，进程树查询也可能被拒绝。旧 GUI 的隔离基线可运行 `work/setup-ui-tests.exe <项目 work 目录> --benchmark`；它只使用测试路径和系统操作替身。

## 核心回归

| 入口 | 覆盖范围 |
|---|---|
| C ABI 和部署核心 | 空参数、中文、引号、长度边界、用户写锁、快捷方式原始内容、写入失败与回滚。保存后分配响应失败时仍返回独立提交状态。 |
| 原生和 Tauri 后台入口 | 对照 8 组结果文件和退出码。给测试进程指定缺失的 WebView2 路径，后台命令仍能完成。 |
| PowerShell 和兼容界面 | 旧 EXE、新能力资源、取消、签名与安装失败。隔离 EXE 验证显式兼容模式和缺失运行库时的回退。 |
| 桌面设置页 | 原生选择器取消后恢复焦点。重复点击只保存一次。保存期间关闭会受到保护。只有当前输入的有效结果才能启用保存。 |
| 交互和无障碍 | 三语切换保留输入。文字淡化，控件边框平滑移动。短窗口、键盘、减少动态效果及错误详情可在隔离窗口复测。 |
| 生命周期 | 检查首次读取和空闲命令计数。正常关闭后，按进程号和创建时间核对所属进程全部退出。 |

早期只读原型的单 EXE 约 8.5 MiB。含 WebView2 的空闲进程树约占 179 MiB 私有内存。原生兼容窗口和 Tauri 使用不同的就绪标记。当前配置器的交互耗时及启动器的查找耗时见[性能对照](performance-review.md)。

运行库安装参考[微软的 WebView2 分发说明](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution)。权限和内嵌 Windows 资源见 [Tauri 权限](https://v2.tauri.app/security/capabilities/) 和 [Tauri Windows 构建接口](https://docs.rs/tauri-build/latest/tauri_build/struct.WindowsAttributes.html)。

## 第 4 阶段默认入口与交付验证

### 交付变化

- 默认构建输出单个 Tauri 配置器和独立 C++ 启动器。旧 GUI 可单独构建，也继续内嵌为兼容模式。没有修改部署核心或新增常驻服务。
- 正式包只包含实际设置页，不包含模拟页面和测试路径入口。主窗口只允许内嵌页面导航。前端、Rust、C++、资源版本及同次构建载荷在构建和交付检查中核对。
- 完整检查接入 `test-build.cmd` 和 `analyze.cmd`。发布脚本先检查，再生成两个 EXE、源码 ZIP 和 SHA-256。测试模式写入新的 `work/release-stage-*`。源码包的文件范围由 [tools/release-sources.ps1](../tools/release-sources.ps1) 定义。
- 三语 README 更新实际操作和构建前提；原生设计与性能文档保留为兼容实现基线。模块地图和交接说明已同步。

### 验证记录

- 原有 C++ 回归、C ABI 隔离检查、44 项前端检查、6 项 Rust 检查、TypeScript、rustfmt、两种 feature 组合的 Clippy 和 MSVC 静态分析通过。覆盖部署、恢复、快捷方式迁移、并发、写入失败与回滚、`.env` 内容保留及桥接分配失败。
- 8 组后台结果与原生入口逐字一致。缺少 WebView2 时仍保留参数、结果文件和退出码，不进入图形初始化。PowerShell 5.1 和 7 的下载及后台检查通过；安装、兼容界面和取消分流使用替身验证。
- 实际桌面检查覆盖显式兼容参数、带引号参数及缺少运行库回退。三次均只有一个进程，没有 WebView 数据或子进程，正常关闭后退出。
- 实际 WebView2 窗口拒绝未授权命令、模拟页面和未打包页面导航。隔离保存保留空参数、中文、引号；重复点击只提交一次，原生关闭命令在保存期间返回忙碌。保存提醒保留，点击完成后无所属进程残留，控制台没有错误。
- 独立模拟原型仍能构建和加载，连续三次正常关闭，无部署写入。单文件资源检查确认图标资源、原 manifest 与同次构建的启动器；正式包没有额外 VC++ 或 WebView 加载器 DLL 依赖。

### 性能对照

以下是本轮隔离短时样本。原生基线使用屏幕外窗口与系统操作替身；新界面使用可见窗口和全新 WebView 数据目录，标记包含首次原生加载与校验。两种启动标记不同，不能作为严格冷启动排名。工作集合计重复统计共享页。

| 项目 | 原生 GUI 基线 | Tauri 实际设置页 |
| --- | --- | --- |
| 配置器单 EXE | 1.23 MiB | 9.28 MiB |
| 就绪标记 | 111 ms | 三次 767–833 ms |
| 进程数 | 1 | 7 |
| 空闲 60 秒后私有内存 | 2.68 MiB | 192.95 MiB |
| 空闲 60 秒后工作集 | 17.99 MiB | 393.02 MiB |
| 空闲业务调用 | 无业务计时器 | 无重复桥接调用 |

新界面的内存开销明显增加。独立启动器仍约 0.51 MiB，没有前端依赖；兼容模式保持单进程。本轮新界面连续三次关闭后全部所属进程退出；后两轮各观察 2 秒，私有内存分别为 195.50 和 191.24 MiB，没有逐轮持续增加。短时样本不能证明长期无资源增长。

### 待验与交付边界

- 用户视觉验收、真实 Windows 150% / 200% 缩放、跨显示器、独立机器和更长时间使用。本机 100% 与限定轮次的反复操作结果见下节。
- 真实 WebView2 安装、实际登录自启、代理连通性和退出 ChatGPT。本轮没有执行这些系统操作。
- 本地测试包保留当前版本号，不覆盖用户部署或旧 Release；本轮不提交、合并、上传或发布。测试目录和性能原始记录只保存在 `work/`。

## 交付后补充验收

本轮在本机继续检查多次开关、较长空闲、重复语言与主题切换，以及不同窗口尺寸和渲染缩放。仅使用隔离功能包；不修改全局显示设置，不执行真实安装、自启、联网或退出 ChatGPT。

生命周期脚本支持 `-Cycles` 与最多 600 秒的 `-IdleSeconds`，默认仍为三次开关和首轮空闲 60 秒。每轮记录整个进程树的内存、句柄、线程、CPU 时间和窗口实际 DPI；记录留在 `work/`。窗口 DPI 与页面渲染缩放分开记录，CDP 模拟不能替代真实 Windows 150% / 200% 或跨显示器验收。

验收条件：空闲无重复业务调用；正常关闭后无所属进程残留；反复操作后没有遗留弹层、临时样式或持续动画；输入保留，短窗口与缩放场景仍能使用主要操作。发现问题时只修复相关模块，并运行对应回归。

### 复测方法

先构建隔离功能包 `frontend/desktop.ps1 -Command harness`。生命周期检查从项目根目录运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/ui-prototype-tests.ps1 -Functional -Cycles 10 -IdleSeconds 180
```

交互检查用 Playwright CLI 附加实际 WebView2。以下命令仅启动隔离包；脚本不点击保存。CLI 缓存和截图留在 `work/`。

```powershell
$root = (Get-Location).Path
$session = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/ui-prototype-tests.ps1 -Action Start -Functional -RemoteDebugging | ConvertFrom-Json
Add-Type -AssemblyName System.Net.Http
$handler = [Net.Http.HttpClientHandler]::new()
$handler.UseProxy = $false
$client = [Net.Http.HttpClient]::new($handler)
try {
    $endpoint = ($client.GetStringAsync("http://127.0.0.1:$($session.port)/json/version").GetAwaiter().GetResult() | ConvertFrom-Json).webSocketDebuggerUrl
} finally { $client.Dispose() }
$env:npm_config_cache = Join-Path $root 'work/npm-cache'
$null = New-Item -ItemType Directory -Force -Path 'work/playwright'
Push-Location 'work/playwright'
try {
    npx.cmd --yes --package @playwright/cli@0.1.24 playwright-cli -s=ui-acceptance attach --cdp=$endpoint
    npx.cmd --yes --package @playwright/cli@0.1.24 playwright-cli -s=ui-acceptance snapshot
    npx.cmd --yes --package @playwright/cli@0.1.24 playwright-cli -s=ui-acceptance run-code --filename=../../tests/ui-interaction-checks.js
} finally { Pop-Location }
```

在同一会话用 `-Action Resize -Functional -Session $session.root -Width 480 -Height 400` 调整外框后，可再运行交互检查。尺寸单位是物理像素，页面可用尺寸会扣除标题栏和边框。结束前先用 `-Action Inspect` 记录所属进程；点击标题栏关闭，检查 Esc 返回和放弃更改，再用 `-Action VerifyClosed` 核对退出。后三个动作也须传入 `-Functional -Session $session.root`。不要把脚本附加到正式配置器。

复测语言亮度和窄窗布局时，另开一个隔离功能窗口，将上例的 `--filename` 改为 `../../tests/ui-responsive-checks.js`。它采样深浅色中“恢复默认”的画面，检查 540 像素断点两侧的换行、焦点和反向拖动，再检查三语菜单及说明窗的边界。

窗口实际 DPI 来自 [GetDpiForWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdpiforwindow)。页面密度由 [CDP Emulation](https://chromedevtools.github.io/devtools-protocol/tot/Emulation/) 临时覆盖，结束后还原；它不验证 Windows 原生控件缩放。

### 补充结果

- 本轮只修改验收脚本、源码包清单和开发文档，没有改产品代码或增加产品依赖。脚本语法、Windows PowerShell 5.1 运行、文档和交付检查通过。上一轮完整编译、C++、前端、Rust 及静态分析结果仍对应当前产品源码。
- 连续打开并正常关闭 10 次，首轮空闲 180 秒，后九轮各 2 秒。每轮加载一次，空闲期间无重复业务调用，关闭后均无所属进程残留。首轮私有内存由 195.16 降至 191.95 MiB；后九轮为 192.49–197.44 MiB，没有逐轮上升。测量未连接调试器，不能据此保证长期无增长。
- 实际窗口为 96 DPI，即本机 100%。在页面 640×760 和短窗口 464×361 两种可用尺寸下，检查三语、深浅色及 1 / 1.5 / 2 渲染密度，共 36 种组合。没有横向溢出；底部操作、帮助弹窗按钮和焦点恢复通过。缩放模拟不替代真实 Windows 150% / 200% 验收。
- 同一窗口累计 90 轮语言、主题、协议和折叠操作。空参数、中文及带引号输入保留；方向键、空格、点击选项文字与帮助 Esc 行为通过。未见遗留过渡样式、持续动画或控制台错误。
- 最后 30 轮的 DOM 节点从 410 到 408，事件监听数保持 405，回收后的 JS 堆约 9.66 到 9.75 MiB。操作后再空闲 60 秒，没有重复后台调用；调试会话进程树私有内存从 315.52 降至 314.24 MiB，句柄从 3773 降至 3771。这是已预热且连接调试器的样本，不与无调试启动基线直接比较。
- 未保存关闭先按 Esc 返回，输入仍在；再次关闭并放弃后，整个所属进程树退出。没有保存部署、安装运行库、更改自启、修改系统代理或关闭真实 ChatGPT。

仍待用户视觉确认、真实 Windows 150% / 200% 及跨显示器、独立机器和真实系统操作。阶段结束后暂停，不提交、合并或发布。

## 无障碍与错误详情补充检查

本轮检查正常和短窗口中的键盘、长输入、错误详情、高对比度及减少动画。只操作隔离功能包，媒体覆盖仅用于当前 WebView2，不改 Windows 设置。判定重点为操作可达、焦点不逃出弹窗、详情可滚动、错误码和处理建议完整，以及输入不丢失。屏幕阅读器和系统级无障碍设置另验。

按上节连接 Playwright CLI，在简体中文界面执行以下命令。脚本不点击保存。先检查正常窗口，再调整隔离窗口外框为 480×400 后重跑。退出和所属进程核验沿用上节步骤。

```powershell
Push-Location 'work/playwright'
try {
    npx.cmd --yes --package @playwright/cli@0.1.24 playwright-cli -s=ui-acceptance run-code --filename=../../tests/ui-accessibility-checks.js
} finally { Pop-Location }
```

模拟采用 [Playwright 页面媒体覆盖](https://playwright.dev/docs/emulation)。键盘检查参考 [WAI-ARIA 模态弹窗模式](https://www.w3.org/WAI/ARIA/apg/patterns/dialog-modal/)。原生主题先完成切换，再固定模拟的系统配色；两者不能同时切换后立即取样。弹窗焦点占位在下一帧跳转，检查须等待实际焦点稳定。键盘聚焦的提示须移开焦点才会收起。

本轮验证结果：

- 在实际 WebView2 的 640×760 和 464×361 页面中，检查减少动画、浅色与深色高对比度配色、增强对比度各与应用深浅色组合，共 16 组。底部操作和弹窗按钮可见，没有横向溢出。系统配色仅在页面内模拟，不代表 Windows 无障碍设置已验收。
- Tab、Shift+Tab、Enter、Space、Esc 及短窗口帮助正文的 End 滚动通过。焦点留在弹窗内，关闭后回到入口。无障碍树中的可用按钮、开关、单选和输入均有名称；未进行真实屏幕阅读器朗读检查。
- 原生校验返回端口错误 87、路径过长错误 206。界面显示错误码、含义、简要解释、处理建议和可展开的技术详情。长参数、中文、空参数及引号保留。减少动画模式没有运行中的动画；检查结束后还原媒体覆盖。
- 控制台无错误或警告。测试没有提交保存；正常关闭后整个所属进程树退出。Windows PowerShell 5.1 的无代理回环调试客户端、脚本语法、文档检查通过。
- 本轮只新增桌面验收脚本并更新源码清单和开发文档，产品源码不变。测试包保留相同 EXE，源码包纳入新增脚本，另存新的本地目录，不覆盖旧包。

四阶段实现和本机隔离检查已完成。上述结果不覆盖真实系统缩放、跨显示器、系统高对比度、屏幕阅读器和独立机器。最新暂缓项目与外部环境边界见顶部清单，不将未验证的项目标为通过。

## 正式包与只读环境补充验收

- 正式单 EXE 在已有 WebView2 上打开实际设置页，能识别现有部署并恢复原输入。页面不含原型工具栏，静默启动保留旧值且不可编辑，原生校验通过。`info` 和保留配置的 `plan` 返回成功；`plan` 的 `committed` 为 `false`。
- 点击真实端口检测后，临时提示显示检测来源，详情保留“检测不代表联网已成功”的说明。详情可关闭，字段没有校验错误。没有提交保存。
- 用页面读出的当前代理发送两次无账号、无 Cookie 的 HTTPS HEAD 请求。两个公开页面均返回 200，TLS 证书校验正常。此项只证明这两次请求的代理连通性，不证明登录、自启、Codex 对话或 WebSocket 行为。
- 项目使用的 WebView2 引导程序完成真实下载，并通过有效微软签名校验。没有执行安装器，没有卸载现有运行库。
- 隔离窗口持续空闲约 57 分钟，只加载一次并执行一次初始校验，没有重复业务调用。结束前进程树共 7 个进程，私有内存约 188.89 MiB。此时尚未附加调试器；没有同窗口的早期内存采样，不能据此判定长期增长趋势。
- 正式窗口和本轮隔离窗口都通过关闭按钮退出，所属进程无残留。既有部署、设置、`.env` 和快捷方式逐项核对未变，记录过的注册表基线值也未变。原始记录只留在忽略的 `work/` 目录，源码包不包含个人配置、快照或备份。
- 本轮没有改产品代码。只更新验收说明，并复核本地测试包。Windows 缩放与系统高对比度按用户要求暂缓；真实安装、登录周期和退出当前桌面应用等项目仍保留明确边界。

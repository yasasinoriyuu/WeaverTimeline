# V7：补齐 CAK 的 Sequencer 内嵌承载

## 原因与边界

用户明确要求继续追溯 CAK 并完善母版。V6 冻结标签保持原状，本版为 `7-dev`。以下主体记录母版追溯阶段，当时不自动同步消费者；后续 CineWeaver 接入、空白点击修复和提交授权见文末追加记录。

此前仅抽取 Viewport Overlay，遗漏 CAK 已有的原生顶层编排轨道。两者不是同一个 UI 位置。CAK 当前 README 第 51 行、DESIGN 第 401 行与 `CharacterActionPlannerTrackSection::CreateViewWidgets` 都明确对应原生轨道嵌入；旧独立宿主说明不能覆盖这条现行代码路径。

追溯对象：CAK 本地 `refactor/canonical-semantic-runtime`，HEAD `11749c4c7b8c5e184bf1c94beec47c11c6ba9ea4`。已有 README 与 lane-layout 修改保持不动。重点读取 TrackEditor、TrackSection、Panel SectionTrack 模式、ViewportOverlay、Playback Track 的布局锚点构造。CAK 文档仍有“锚点无运行时求值”旧描述，但当前 Track 实际创建 Runtime 模板；本轮只提取 UI 承载，不把旧描述当其运行时事实。

## 母版与消费者分工

| 层 | 责任 |
| --- | --- |
| Core `FWeaverSequencerSection` | 通过 UE 公共 Section overlay 接口挂载 editable widget，避免重复挂载；动态高度、重挂取消、销毁解绑 |
| Core `SWeaverEditableTimeline` | 内嵌布局、现有 V6 Adapter/事务/权威回读；关闭后禁用；终止式 Shutdown |
| Core `FWeaverSequencerBridge` | 显式绑定所属 ISequencer；时间/ViewRange 联动；close/activate 通知；绝不回退到另一窗口 |
| 消费者 Runtime 或持久化层 | 真实顶层 Track、全时段锁定的布局 Section、业务 Section 与资产；业务求值不能依赖 UI 存活 |
| 消费者 Editor | 注册 TrackEditor、明确用户创建入口、左侧业务行名/菜单、向 Adapter 通知业务来源变化 |
| Reference | 无角色/相机依赖的真实顶层 Track 与可撤销文档，用于执行原生创建、输入、销毁验证 |

Core 仍是私有源码复制包，没有新增反射类、共享插件或 Runtime→Editor 依赖。新增 `WeaverSequencerSection.h/.cpp`，完整 Core 为 16 个文件。

## 接入

先注册匹配的 TrackEditor，再创建顶层 Track。参考 `Reference/Source/WeaverTimelineReference/Private/WeaverReferenceTrackEditor.cpp`。顶层 Track 使用不带 Object Binding 的 `MovieScene->AddTrack<...>()`；不把标题轨道加在 Camera Binding 下。

消费者在明确创建行为中建立一个 `Range=All()` 且 `SetIsLocked(true)` 的布局锚点。它不承担 Motion 时间，不应拉长业务 Section 来冒充布局。

```cpp
auto Timeline = SNew(SWeaverEditableTimeline)
    .Adapter(Adapter)
    .EmbeddedInSequencer(true)
    .SequencerSource(GetSequencer());
return MakeShared<FWeaverSequencerSection>(Section, Timeline, GetSequencer());
```

传入同一个 TrackEditor 的 Sequencer。内嵌模式不发现全局窗口，即使 weak pointer 已失效也不会连到别的窗口。原来的 `SyncSequencer(true)` 发现模式仍保留用于旧 Viewport 消费者。

内嵌模式去除重复时间尺和占用时间区域的左侧标签宽度；左侧业务行名及按钮由消费者的原生 Outliner 布局提供。当前参考例只展示一行，不宣称已经完成 CineWeaver 的多相机树形 Outliner。

锚点锁定用于阻止原生 Section 移动；`IsReadOnly=false` 保证子控件接收鼠标。这不代表放弃业务只读判断：Adapter 仍须拒绝只读资产写入。Native Track 删除政策属于消费者；Core 的 Section 不可删除不等于禁止用户删除整条原生 Track。

高度来自同一 Timeline 布局计算，变化时通知所属 Sequencer 刷新；不改变 MovieScene 数据、不扩大资产范围。不要自行嵌套 Begin/End 拖动事务。

显式绑定在初始化时只连接数据和 delegate；不得在 `MakeSectionInterface` 内读取 `GetSequencerWidget()`，此时 UE 可能尚未完成 SSequencer 构造。内嵌模式直接使用原生 Section 的几何，不再查找 Viewport 的 TrackArea 来补偿位置；非内嵌的显式绑定到首次 Slate Tick 才发现几何。

## 验证记录

测试宿主：`F:\codex\cache\WeaverV7Host\WeaverReference.uproject`，UE 5.8.1，CL 56057345。仅 F 盘独立缓存目录，复制母版 Core 后逐文件校验。构建目标 `WeaverReferenceEditor`；不是生产工程。

失败证据保留在 F 盘缓存，未计作通过：

- `weaver-v7-tests1.log`：原生 Track 创建时触发 SharedPointer 断言，定位到 Bridge 注册时读取未建好的 SSequencer。移除原生构造期间的 Widget 读取。
- `weaver-v7-tests2.log`：同一阶段的本地时间基准尚未初始化，转换时 SourceRate.Numerator 为零；增加正数分子和合法分母检查，首次正常 Tick 回读。UE 的 `FFrameRate::IsValid()` 只检查分母，不能单独依赖它。
- `weaver-v7-tests3/4`：原有 10 组通过，新增 2 组因原生虚拟化轨道行尚未生成而失败。测试改为正常编辑器循环中的有界 latent 等待，5 秒超时仍明确失败，不绕过原生 Widget 去测试替身。
- `weaver-v7-native-initial.png`：实际渲染显示嵌入成功，但时间轴横向偏移；按 CAK SectionTrack 分支去除重复的 Viewport 横向补偿。
- `weaver-v7-visual.log`：关闭参考窗口后退出编辑器，模块又关闭同一个 Sequencer，触发退出崩溃（用户截图）。参考宿主改为窗口关闭时释放，EnginePreExit 提前清理，模块退出幂等；不向 Epic 发送崩溃报告。该缺陷属于本轮参考宿主，正式工程未参与。
- `weaver-v7-build7.log`：崩溃报告进程临时占用符号/二进制，链接失败。关闭本次报告窗口后重新成功构建；没有使用失败构建后的旧 DLL 运行测试。
- `weaver-v7-tests5`：即使显示测试窗口并让出编辑器循环，NullRHI 仍不生成实际虚拟化轨道视图。因此原生嵌入测试必须启用渲染，不能用 NullRHI 替代；5 秒等待超时保留为失败。
- `weaver-v7-rendered-tests`：原生输入开始实际执行，发现测试在 Undo 后仍持有旧 WidgetPath，阻止 SSequencer 释放。改为每阶段发现当前树，关闭前释放祖先路径引用。
- `weaver-v7-rendered-tests2`：数据显示初次控件已生成但实际高度仍为 0；等待真实布局，不放宽高度要求。另捕获到取消只释放固定 CursorPointerIndex、遗留输入实际捕获指针的问题；Core 改为记录本次 User/Pointer 身份并只释放自己的捕获，不释放别的控件。

本轮验证结果（2026-09-18）：

| 检查 | 结果与证据 |
| --- | --- |
| UE 5.8.1 构建 | 通过，`weaver-v7-build13.log`；后续仅调整参考窗口初始化栏宽，`weaver-v7-build14.log` 再次构建通过 |
| 启用渲染的自动化 | `weaver-v7-rendered-tests3/index.json`：12 组通过、0 失败、0 未运行。原有 10 组加 2 组原生嵌入测试 |
| 实际原生 Widget 路径输入 | 自动化通过：20～60 帧片段拖动至 50～90，独立回读文档及显示；Undo/Redo 后重新发现当前控件再核对 |
| 拖动中关闭所属 Sequencer | 自动化通过：捕获和活动编辑立即取消，延迟 Finish 不能提交；两个真实 Sequencer 的时间与关闭状态不串联 |
| 渲染画面 | 已查看 `weaver-v7-native-reopen.png` 和最终构建的 `weaver-v7-native-final3.png`：原生顶层轨道名及片段可见，没有额外时间尺，20～60 帧与原生时间尺对齐；栏宽改在构造前设置，首次打开即生效 |
| 关闭、重开、退出 | `weaver-v7-visual2.log`：MCP 关闭参考窗口，主窗口输入命令重开，截图确认新窗口，再正常退出编辑器。PID 23220 退出、8000 释放，日志正常关闭；此前同一路径的重复 Close 崩溃未复现 |
| 最终构建退出复测 | `weaver-v7-visual3.log`：先关闭参考窗口，再关闭主编辑器；PID 23320 正常退出、8000 释放，无新 CrashReportClient。两份可视验证日志均无 Fatal/Assertion/Ensure/Unhandled Exception |
| 物理鼠标与正式工程验收 | 未测。Slate 路由输入及 MCP 窗口操作不冒充物理鼠标验收；CineWeaver 消费者迁移尚未执行 |

自动化必须运行 `UnrealEditor.exe` 并启用渲染，使用 `-ExecCmds="Automation RunTests WeaverTimeline" -TestExit="Automation Test Queue Empty"`；不得添加 `-NullRHI` 来替代原生窗口测试。自动化的 TestExit 不代替正常退出检查，后者由独立的可视编辑器流程覆盖。

证据均在 `F:\codex\cache`，不纳入源码。母版 Core 与宿主复制件逐文件核对；本轮保留 V6 冻结标签，不修改 CAK，不迁移 CineWeaver，不向 E 盘同步，不提交或推送 Git。

## 后续：CineWeaver 空白点击策略（2026-09-18）

用户试用 CineWeaver 后明确要求：编排行空白点击/拖动不移动播放头，块头尾轻点仍跳到端点；随后授权本任务修改正式母版，并在完成后 commit、推送 GitHub。上述“未迁移、未提交”仅描述先前追溯阶段，不是当前状态。

Core 增加 `AllowTrackAreaScrub`，默认为 `true`，保证已有消费者原行为。CineWeaver 显式传 `false`；空白左键清除选择后立即结束，不捕获鼠标、不发送时间变化。独立控件本地时间尺保持仅显示行为，原生 Sequencer 的外部时间尺由引擎处理。母版完整 16 文件同步 CineWeaver，没有消费端私改 Core。

新增 `WeaverTimeline.Boundaries.TrackAreaScrubPolicy` 检查默认兼容、关闭空白 scrub、重复点击/拖动、无瞬间时间通知、清除选择、右键菜单和本地时间尺原行为。首次运行 `weaver-track-scrub-tests` 时，测试错误假定本地时间尺可点击跳时间，两条断言失败；核对既有 `HitTest` 确认时间尺一直仅显示，修正测试前提，未修改业务行为或放宽时间容差，失败报告保留。

最终母版构建 `weaver-track-scrub-build2.log` 成功；启用渲染的 `weaver-track-scrub-tests2/index.json`（PID 15976）13 组通过、0 失败、0 未运行。其中 1 组成功但带 `r.MotionVectorSimulation` 渲染线程访问警告，未把警告隐藏或称为完全无警告。测试包含既有生命周期、原生嵌入及新空白点击策略。测试进程已退出。

CineWeaver 同一个真实原生 Sequencer 输入用例在修复前检测到空白点击/拖动错误地移到 100/120 帧，修复后保持预期 32 帧；同时确认块体选择、端点跳转、拖边改到 75 帧均正确。详细证据由 CineWeaver 的 `Docs/PLAYHEAD_CLICK_POLICY.md` 保存。Slate 路由输入不冒充完整人工鼠标验收。

本次提交包含先前已验证的 V7 原生承载及其说明文档，因为消费者和点击策略依赖这些尚未提交的源码。冻结标签 `v6-frozen` 保持不动；不修改 CAK、AudioHead、E 盘工程或用户试用资产。

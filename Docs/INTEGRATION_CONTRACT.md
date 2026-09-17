# WeaverTimeline V6 集成契约

## 默认入口与模块边界

Core 是复制进消费者 Editor Module 的源码母版，默认使用 SWeaverEditableTimeline。SWeaverTimeline 保留底层绘制和回调 API，直接使用它不具有 V5 的提交闭环保证。

| 层 | 所有权 |
| --- | --- |
| SWeaverTimeline | Slate 绘制、HitTest、输入捕获、局部拖拽预览 |
| SWeaverEditableTimeline / FWeaverTimelineEditController | Session、终止、提交后的全量回读、Selection/Expansion 协调 |
| IWeaverTimelineEditAdapter | 权威数据映射、业务规则、事务对象、外部预览目的地 |
| FWeaverSequencerBridge | 可选时间、ViewRange、轨道几何同步 |
| Host / Overlay | 可选布局、折叠、Viewport attach/detach |

Core 没有反射类型或模块导出宏。参考插件的 transactional UObject、测试 Tab 注册留在 Reference，不复制到业务消费者。依赖单向为 Consumer → Core → UE Editor/Slate。

## 接入

构造 SWeaverEditableTimeline，传入 Adapter；可选 SyncSequencer(true)、DisplayRateProvider、OnFrameChanged。Overlay 使用 RegisterEditable(Timeline, Label)，自动绑定隐藏/折叠/卸载取消。

OnBlockEndpointClicked(LaneId, BlockId, bStart) 将端点身份交给消费者。默认 JumpToEndpointOnClick(true) 同时跳转播放头；设为 false 可仅接收端点事件，由消费者实现属性编辑等业务行为。普通 Scrub 不发送端点事件。

实现三个必需方法：GetContext、BuildPresentation、Commit(Session)。消费者不再连接 Finished 后的 SetBlocks/SetKeys。

## Proposal 与权威结果

固定流程：

```text
HitTest → BeginEdit → Original + Proposal + SessionId + Context
       → BeginPreview → UpdatePreview（零或多次）
       → BeginTransaction → Commit → Transaction.Finish
       → EndPreview → BuildPresentation → SetPresentation → OnReconciled
```

Cancel 跳过事务和 Commit，仍执行 EndPreview 和全量回读。提交返回 Applied / Rejected / NoChange；控制器取消返回 Cancelled。Applied 允许 Snap、Clamp、冲突调整和修改多个对象；最终值不必等于 Proposal。Reason 是业务说明，不是 UI 几何。

BuildPresentation 每次读取完整当前权威状态。Key/Block ID 必须有效且稳定，在各自类型内唯一；LaneId 引用存在的 Lane；Timing Row ID 在所属 Block 内唯一。禁止用数组索引生成身份。

## Session、数据源与外部预览

- 所有调用和 OnSourceChanged 广播在编辑器/Slate 线程执行。
- GetContext 返回数据源身份和单调递增的 Revision。切换资产/Sequence/文档时更换 Context.Id；外部修改、删除、Undo/Redo 时递增 Revision 并广播 OnSourceChanged。Revision 放在非事务状态中，不随 Undo 回退。
- Session 保存开始时的 Context、Original、当前 Proposal 和独立 SessionId。权威变化取消旧 Session，旧 MouseUp 不得写入新数据源。
- BeginPreview/UpdatePreview 只修改临时预览。EndPreview 按 Session.Id/Context 清理原目的地，在成功、拒绝、取消、隐藏、销毁后都调用一次。即使消费者已切换对象，也必须能清理旧 sink。
- Cancel 不得把 Original 写回当前权威对象。权威值可能已被外部更新；取消只清理 preview 并重读。
- 回调中允许发 SourceChanged 或请求刷新。控制器串行处理；终止前已移除活动 Session，重入 Finished 不会再次 Commit。
- 读取回调应当纯读。读取期间 Context 变化时不发布混合快照，下一次 Tick 重试。Tick 检测漏发事件但 Revision 已变化的来源；它无法检测既不通知也不更新 Revision 的业务写入。

## Transaction / Undo / 通知

消费者可实现 BeginTransaction(Session)，返回 IWeaverEditTransaction。Core 只在提交瞬间创建 scope，在 Commit 后调用 Finish，随后销毁。参考实现使用 FScopedTransaction，在写入前调用 Document.Modify；Rejected/NoChange 取消空事务。

Core 在创建事务前和创建事务返回后检查取消、解绑及数据源变化。BeginTransaction 回调中取消或隐藏时，不调用 Commit，已创建的 scope 以 Cancelled 结束，EndPreview 保留取消原因并且只执行一次。Commit 开始后已进入消费者写入代码，此时的重入取消不意味着自动撤销已经发生的业务写入。

Rejected/NoChange 必须保持权威数据不变；若业务先写后失败，业务自身应回滚。Core 无法替未知资产撤销任意副作用。默认无需在整个鼠标拖拽期间打开事务。

Undo/Redo 完成后消费者广播 OnSourceChanged；参考 UObject 在 PostEditUndo 中通知适配器。Core 自动回读并发出 OnReconciled，业务缓存/实时求值系统可订阅这个已稳定的 Presentation 通知。通知附带的是最后一次编辑结果；外部刷新本身不等同于新 Commit。

## 所有修改共用入口

SWeaverEditableTimeline 把 Delete/Backspace 自动转成 CommandId=Delete；Lane Header Action 自动转成对应 ActionId。消费者在 Commit 中解释命令。

业务右键菜单只负责呈现菜单。Create/Duplicate/Paste/属性修改等菜单动作调用 Controller.ExecuteCommand，传入 Target=Command、CommandId 和目标/帧字段。这个入口也保证提交、清理和刷新。菜单布局和弹出位置约束由创建它的消费者负责。

## Selection、Expansion 与 teardown

同一 Context 内按稳定 ID 保留选择；对象换 Lane 时更新所选 LaneId，对象被删除时清除并通知。Context 改变清选择和展开覆盖；已删除 Block 的展开记录也被清理。

使用 Overlay.RegisterEditable 自动在隐藏、折叠、Viewport detach、Unregister 时取消编辑。手工构造 Host 时将 OnDeactivated 绑定到 EditableTimeline.Deactivate；自定义 Tab 在关闭/隐藏时调用 Deactivate。任意外部父 Widget 的隐藏不是 Core 可观察的通用事件，消费者须使用这个明确的生命周期入口。EditableTimeline 析构会 detach 控制器、取消编辑，并解绑留存子 Widget 的回调。

鼠标按下过程中的选择或 Scrub 回调若取消交互，该次按下不再捕获鼠标。消费者回调可以同步隐藏控件或切换数据源，不必等待 MouseUp 清理。

## 时间与视图

SyncSequencer(true) 自动注册 Bridge、传递 Scrub/Pan/Zoom、在 Tick 镜像 ViewRange/TrackArea/播放 SubFrame，并在 Sequencer 绑定或 focused Sequence 变化时取消编辑。默认发现策略仍是 LevelEditor integration 中第一个有效 Sequencer，不代表支持任意多个窗口的用户意图判定。

默认 SyncSequencer(false) 完全关闭联动：Tick 不发现或绑定 Sequencer，Scrub/Pan/Zoom 不写入它。直接使用 Bridge 时也必须先 Register；Unregister 后 Sync 和推送均无效，重新 Register 后恢复双向同步。

数据源与哪个 Sequence 对应由 Adapter 决定；业务上下文变化时必须更新 Context。Bridge 取消保护不替代数据身份规则。SetCurrentFrame 可供外部时间源更新播放头；它不产生反向通知。

## 依赖、版本与迁移

复制全部 Core 文件到消费者 Editor Module 私有目录。实际依赖见 Reference 模块 Build.cs：Core、CoreUObject、Engine、Slate、SlateCore、InputCore、UnrealEd、LevelEditor、MovieScene、Sequencer、SequencerWidgets。没有 Runtime → Editor 依赖。

V3 底层交互 API 保留。V4 试验性分散 CommitKey/CommitBlock/CommitTimingRow 适配器升级为统一 Session/Proposal Commit；Accepted 必须等于 Proposal 的规则已废弃。V6 在 V5 基础上修复四处边界并增加可选端点通知，不改变必需 Adapter 方法。旧消费者须迁移 Adapter，不能仅同步 Core 就宣称完成集成。Reference 额外依赖 LevelSequence，用于创建真实 Sequencer 的隔离测试，不是 Core 的新增依赖。

验证方式和范围见 [Reference README](../Reference/README.md)。

# WeaverTimeline product specification

## 定义

WeaverTimeline 是一套 **UE5 编辑器里的通用自绘编排系统源码母版**。

它解决的不是某一种业务，而是统一“在时间上摆放、选择、拖动和调整东西”的交互语言。CharacterActionKit、CineWeaver、AudioHead 等插件可以拥有不同业务含义，但用户面对的 Lane / Key / Block / 时间尺 / 播放头和鼠标操作应保持一致。

WeaverTimeline 不是共享运行时插件。每个业务插件携带自己的 Core 源码副本，插件之间不形成依赖。

## 用户看到什么

编排系统由四个可感知部分组成：

1. **时间尺与播放头**：显示当前时间位置，并允许 Scrub。
2. **Lane**：每一行代表一个由业务插件定义的编排对象。
3. **Key**：表示某个确定时间点上的离散状态或事件。
4. **Block**：表示一个具有开始和结束时间的区间。

当使用 CAK 风格的 Viewport 承载方式时，编排器位于活动 Level Editor Viewport 底部，可以展开、折叠和拖动改变高度。

V7 另提供 CAK 的原生 Sequencer 轨道内嵌承载：在消费者建立的顶层轨道右侧放置自绘编排。两种承载不是同一个入口，也不能以视口面板替代产品明确要求的 Sequencer 顶层位置。内嵌模式使用所属 Sequencer 的原生时间尺，不重复占用左侧时间宽度。

## 统一交互要求

所有消费者应尽可能保持同一种操作手感：

- 点击 Key / Block：选择。
- 拖 Key：修改时间。
- 拖 Block 中部：整体移动区间。
- 拖 Block 左右边缘：修改区间开始/结束。
- Delete / Backspace：请求删除当前选中项。
- 拖动空白时间区域：Scrub。
- RMB 拖动时间区域：水平 Pan。
- 鼠标滚轮：Zoom。
- RMB 点击 Key / Block：向业务层请求上下文菜单。
- Escape 或 Mouse Capture 意外丢失：取消当前编辑手势，而不是留下半完成状态。

## 与 Sequencer 的关系

WeaverTimeline 是自绘编辑视图，不实现业务动画 Channel；它可以嵌入消费者的 Sequencer 原生 Track/Section UI。

当消费者启用 Sequencer Bridge 时：

- Sequencer 是时间与可视范围的外部权威来源；
- WeaverTimeline 与其 CurrentTime 和 ViewRange 双向同步；
- 自绘轨道区域在横向上应与 Sequencer TrackArea 对齐；
- 用户在 WeaverTimeline 中 Scrub / Pan / Zoom 时，修改请求传给 Sequencer，再由 Sequencer 的结果回写到编排条；
- 业务数据是否存进 MovieScene、资产或其他容器，不属于 WeaverTimeline 的决定范围。

## 数据语义边界

Core 只认识：

```text
Lane
Key
Block
Time
Selection
Edit gesture
Edit session / Context / Revision
Edit proposal / Commit outcome
Authority reconciliation / Preview lifecycle
```

Core 不解释这些对象“是什么”。例如：

```text
CAK Action Block       -> FWeaverBlock
CineWeaver StateKey    -> FWeaverKey
AudioHead nod region   -> FWeaverBlock
```

业务插件负责数据转换、持久化、Undo/Redo、运行时求值和最终业务效果。

## 稳定身份

`LaneId / KeyId / BlockId` 是业务项的稳定身份。数组顺序只表示显示顺序，不能作为业务身份。

这条规则保证 Lane 重排、插入或删除后，已有 Key / Block 不会因为索引变化而指向错误对象。

## 源码母版规则

`Core/` 是唯一母版。

三个消费者里的 Core 副本必须 byte-for-byte 相同。消费者不得直接修改自己的副本来加入业务特例；如果需求属于真正通用的编辑交互，应先修改母版，再统一同步。如果需求只属于一个插件，则放在该插件 Adapter 中。

## 完成标准

WeaverTimeline 的通用抽取完成，至少意味着：

- 自绘 Timeline 本体独立于 CAK / Camera / Audio 业务；
- Lane / Key / Block 通用编辑成立；
- Viewport Host / Overlay 可以承载任意业务面板；
- Sequencer 时间、ViewRange 和 TrackArea 几何同步可以由通用 Bridge 完成；
- Core 中不存在业务专属全局注册或运行时数据所有权；
- 任一消费者只通过 Adapter 映射自己的业务数据，不需要修改 Core；
- 默认编辑路径在任何终止后清理 Preview 并回读完整权威数据，不能漏接 MouseUp 刷新；
- Proposal 允许被业务吸附、限制、拒绝或引发多对象修改，最终 UI 以权威数据为准；
- 外部修改、Undo/Redo 和上下文切换通过统一通知取消旧 Session、刷新 UI；
- 独立 Reference Consumer 验证真实 UObject 事务和 Slate 输入到权威回读的闭环；
- 源码副本可以通过同步脚本和 SHA-256 校验保持完全一致；
- 最终必须在真实 UE 消费者插件中按本机 UE5 Skill / 工程规则完成编译验证。

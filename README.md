# WeaverTimeline

WeaverTimeline 是一份 **UE5 Slate 编排系统源码母版**，不是 CharacterActionKit、CineWeaver、AudioHead 共同依赖的第四个插件。

它的目标是：多个业务插件各自携带一份相同的编排系统源码，从而保持 UI 和交互一致，同时运行时完全独立。

```text
WeaverTimeline (source master)
        |
        +--> CharacterActionKit/Private/WeaverTimeline
        +--> CineWeaver/Private/WeaverTimeline
        +--> AudioHead/Private/WeaverTimeline
```

## Source Version

当前源码母版版本：`5`。

## Core 组成

### 默认编辑入口

`SWeaverEditableTimeline` 与 `FWeaverTimelineEditController` 负责 Session、Commit/Cancel、外部 Preview 清理、全量权威回读和选择状态协调。业务 Adapter 提供数据、提交规则与可选事务 scope；Snap/Clamp/Reject/级联修改后，最终画面均读取权威数据。

接入契约见 [INTEGRATION_CONTRACT.md](Docs/INTEGRATION_CONTRACT.md)。事务型参考插件与 UE Automation 测试见 [Reference](Reference/README.md)。

### 1. 自绘编排条

- `SWeaverTimeline.h/.cpp`
- `WeaverTimelineTypes.h`

负责：

- 时间尺与播放头
- 多 Lane
- Key
- Block
- Selection / Hover
- Key 拖动
- Block 整体拖动
- Block 左右边界 Resize
- Block Endpoint click 与 Resize threshold
- Delete / Backspace 删除请求
- Scrub
- RMB 拖动水平 Pan
- 鼠标滚轮 Zoom
- 右键菜单请求入口
- 空白 Lane RMB context 请求
- Lane Header Action 请求
- 可展开 Timing Rows、动态 Lane 高度与比例 Handle 编辑
- 外部 ViewRange 模式
- Mouse Capture / Escape / CaptureLost 生命周期

### 2. Viewport 底部承载壳

- `SWeaverTimelineHost.h/.cpp`
- `WeaverViewportOverlay.h/.cpp`

来源于 CAK 已经实际工作过的 Level Editor Viewport 底部 Planner 结构，负责：

- 在活动 Level Editor Viewport 底部显示任意编排内容
- 展开 / 折叠
- 拖动调整高度
- 鼠标 Capture 清理
- 用户切换活动 Viewport 后重新挂载

它不理解任何业务数据。

### 3. Sequencer 同步桥

- `WeaverSequencerBridge.h/.cpp`

来源于 CAK 已验证的 Sequencer 联动逻辑，负责：

- 发现并绑定活动 Level Editor Sequencer
- Sequencer CurrentTime -> 编排条时间
- 编排条 Scrub -> Sequencer LocalTime
- Sequencer ViewRange -> 编排条可视范围
- 编排条 Pan/Zoom 请求 -> Sequencer ViewRange
- 自绘轨道区域与 Sequencer `TrackAreaView` 的横向几何对齐
- 播放期间补充 SubFrame 时间同步

业务插件仍然拥有自己的当前帧状态和业务求值；Bridge 只做编辑器时间/视图同步。

## Core 不负责什么

Core 不认识也不持有以下业务概念：

- Camera / StateKey / Orbit
- CharacterAction / Pose / ControlRig
- Audio / 点头 / 摇头
- MovieScene 业务持久化
- Runtime 求值
- Undo/Redo 的业务语义
- 插件专属 Tab / ToolMenu / StyleSet 等全局注册

业务插件通过 `IWeaverTimelineEditAdapter` 映射自己的数据并执行 Proposal。Core 接管通用编辑生命周期，具体业务语义仍属于消费者。

## 为什么不用共同插件依赖

三个业务插件需要能够各自独立启用、禁用和发布。因此 WeaverTimeline 只作为源码母版。三份 Core 应保持 byte-for-byte 一致；每个业务插件自己的 Adapter 可以不同。

`tools/Sync-WeaverTimeline.ps1` 会同步 `Core/` 下的全部源码文件；`tools/Verify-WeaverTimeline.ps1` 会用 SHA-256 校验每份副本是否与母版完全一致。

## 集成方式

把 `Core/` 下的源码原样复制到目标 Editor Module 的私有目录，例如：

```text
Source/MyPluginEditor/Private/WeaverTimeline/
```

不要修改复制后的 Core；业务差异放在插件自己的 Adapter / Panel 中。需要修改通用交互时，先改本仓库母版，再统一同步。

### 基础依赖

自绘 Timeline / Host 需要标准 Editor Slate 依赖，例如：

```text
Core
Slate
SlateCore
InputCore
```

使用 Level Viewport Overlay 与 Sequencer Bridge 时，还需要消费者 Editor Module 按实际 UE5 版本加入相应编辑器依赖。CAK 当前已验证实现使用了 `UnrealEd`、`LevelEditor`、`MovieScene`、`Sequencer`、`SequencerWidgets` 等模块；具体依赖以消费者本机 UE5 Skill 和编译结果为准。

## 当前状态

V5 在已有自绘 Timeline、Viewport Host/Overlay 和 Sequencer Bridge 上补齐编辑闭环，并自带独立参考消费者。根目录仍是源码母版；`Reference/` 是测试插件。运行和验证记录见 [Reference README](Reference/README.md)。

这不等于 CAK/CineWeaver/AudioHead 已完成 V5 迁移，也不等于真实鼠标、Sequencer 多窗口和业务实时预览已通过人工验收。

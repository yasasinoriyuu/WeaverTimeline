# WeaverTimeline

WeaverTimeline 是一份 **UE5 Slate 编排条源码母版**，不是三个业务插件共同依赖的第四个插件。

目标很明确：CharacterActionKit、CineWeaver、AudioHead 等插件都可以把 `Core/` 下的源码原样复制进自己的 Editor Module，从而拥有完全独立的运行依赖，同时保持编排条交互代码一致。

## Core 负责什么

- 时间尺与播放头
- 多 Lane
- Key
- Block
- 单项选择与 Hover
- Key 拖动
- Block 整体拖动
- Block 左右边界 Resize
- Delete / Backspace 删除请求
- Scrub
- RMB 拖动水平 Pan
- 鼠标滚轮 Zoom
- 右键菜单请求入口
- 外部 ViewRange 同步模式
- 完整 Mouse Capture / Cancel 生命周期

## Core 不负责什么

Core 不认识也不持有以下任何业务概念：

- Camera / StateKey / Orbit
- CharacterAction / Pose / ControlRig
- Audio / 点头 / 摇头
- MovieScene 持久化
- Runtime 求值
- Undo 的业务语义
- Tab、ToolMenu、StyleSet 等全局业务注册

业务插件只把自己的数据映射为 `FWeaverLane / FWeaverKey / FWeaverBlock`，并通过 Delegate 接收编辑请求。

## 为什么不用共同插件依赖

三个业务插件需要能够各自独立启用、禁用和发布。因此 WeaverTimeline 只作为源码母版：

```text
WeaverTimeline (source master)
        |
        +--> CharacterActionKit/Private/WeaverTimeline
        +--> CineWeaver/Private/WeaverTimeline
        +--> AudioHead/Private/WeaverTimeline
```

三份 Core 应保持 byte-for-byte 一致；每个业务插件自己的 Adapter 可以不同。

## 集成方式

把 `Core/` 中的文件复制到目标 Editor Module 的私有目录，例如：

```text
Source/MyPluginEditor/Private/WeaverTimeline/
    SWeaverTimeline.h
    SWeaverTimeline.cpp
    WeaverTimelineTypes.h
    WeaverTimelineVersion.h
```

目标 Editor Module 需要已有 `Core`、`Slate`、`SlateCore`、`InputCore` 依赖。Core 本身没有模块 API 宏，也没有 UCLASS/USTRUCT 反射类型，因此同一 UE 工程中多个插件可以各自编译一份而不形成运行时依赖。

`tools/Sync-WeaverTimeline.ps1` 用于从母版同步源码；`tools/Verify-WeaverTimeline.ps1` 用 SHA-256 校验多个插件中的副本是否与母版一致。

## 当前状态

当前仓库建立的是 **源码母版基线**。行为来源于 CAK 已验证的自绘 Planner 交互模式，并吸收 CineWeaver 第一轮业务无关抽取的经验；本仓库本身不是可直接启用的 `.uplugin`，因此这里不宣称已经完成 UE 编译验证。下一步应由实际消费者插件按各自 UE5 Skill / Build 规则接入并编译。

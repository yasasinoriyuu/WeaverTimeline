# WeaverTimeline V6 修复与验证记录

后续状态：2026-09-18 用户确认验收并同意冻结 V6，见 [冻结记录](FREEZE_V6.md)。以下保留原始自动化验证范围，不将用户确认改写为代理执行的逐项人工测试。

日期：2026-09-18（北京时间）。基于 V5 提交 `18bdfefcb595069b033b6c943aa0f5cb82949342`，修复复审指出的四处边界。保留现有架构；本轮只修改 F 盘母版和独立参考测试工程，未迁移正式消费者或修改 E 盘 Harness。

## 使用者能看到的变化

| 问题 | 修复后的行为 | 专项测试 |
| --- | --- | --- |
| 关闭联动仍会影响旁边的 Sequencer | 默认关闭时，刷新、拖播放头、平移、缩放都不改变 Sequencer；明确启用后才同步，解绑后停止 | Boundaries.SequencerOptIn |
| 保存准备过程中取消，仍可能写入 | 在真正写入前取消、隐藏、解绑或更换来源，均不写入；清理预览与空事务一次，并保留取消原因 | Boundaries.CancelBeforeCommit |
| 点击片段端点的信息传不到业务插件 | 默认入口公开端点事件，包含轨道、片段和左右端身份；消费者可关闭默认跳转播放头 | Boundaries.EndpointNotification |
| 取消拖播放头后仍可能占用鼠标 | 按下过程中的 Scrub 或选择回调取消后，不再重新捕获鼠标 | Boundaries.ScrubCancellation |

源码版本递增到 6，便于消费者核对复制的版本。必需 Adapter 方法保持不变，接入说明见 [集成契约](INTEGRATION_CONTRACT.md)。

## 实际验证

- UE 5.8.1，CL 56057345；独立 WeaverReferenceEditor，Win64 Development。
- 引擎 .NET 10 / UBT、VS 14.44，参数 -NoUBA；最终 BuildV6.log 为 Result: Succeeded。首次构建发现新增测试缺少 AnimatedRange.h，补齐后重新构建通过。
- 14 个 Core 文件逐一校验 SHA-256，与母版完全一致。
- UE Automation 筛选 WeaverTimeline：成功 10、带警告成功 0、失败 0、未运行 0。包含新增 4 项和既有 6 项全部回归。
- 测试日志结束于 UTC 2026-09-17 17:18:47（北京时间 2026-09-18 01:18:47），记录 10 tests performed、Test Queue Empty、退出状态 0；本次 PID 23084 已退出。

完整测试项目与行为矩阵见 [Reference README](../Reference/README.md)。Sequencer 专项创建真实 ULevelSequence 和 ISequencer，检查禁用、双向时间同步、视图推送、解绑和重新注册。鼠标专项通过实际 Slate Widget 树路由合成指针事件，并检查返回回复和实际 Capture 状态；端点测试检查收到的身份与最终播放头位置。

本机证据位于独立测试目录，不提交构建产物或日志：

- [构建日志](F:/codex/_WeaverReferenceValidation/BuildV6.log)
- [自动化报告](F:/codex/_WeaverReferenceValidation/ReportV6/index.json)
- [自动化日志](F:/codex/_WeaverReferenceValidation/AutomationV6.log)
- [Core 同步校验](F:/codex/_WeaverReferenceValidation/SyncV6.txt)

## 结论的范围

本轮四处问题已修复，并在真实 UE 环境中通过对应自动化及既有回归。运行方式为 unattended / NullRHI，不代表物理鼠标、屏幕像素、高 DPI 或完整业务插件已验收。Sequencer 多窗口意图、实际角色/相机预览、正式消费者 Adapter 和资产磁盘保存/重载仍需各自验收。

“提交前取消”指尚未进入 Adapter.Commit 的阶段。业务 Commit 已经开始执行后，Core 不自动撤销未知业务副作用；消费者仍须遵守拒绝/无变化不修改权威对象的契约。

# WeaverTimeline V5 验证记录

日期：2026-09-18（北京时间）。本次完善的是 F 盘 WeaverTimeline 源码母版，保留开始工作时已有的 Batch 1 修改。实现与验证阶段未执行 Git 提交或推送；随后按用户要求将源码与说明文档一并备份到现有开发分支。不合并 PR，不迁移正式消费者，也不修改 E 盘临时 Harness。

## 实现结果

- 默认入口升级为 SWeaverEditableTimeline + EditController，消费者提供统一 Adapter。
- Session 保存身份、开始 Context/Revision、Original 与 Proposal；源变化、隐藏、销毁及输入取消会终止编辑。
- Commit 后一律清理 Preview 并全量回读 Authority。Applied 允许吸附、限制和级联修改，不要求最终值等于鼠标 Proposal。
- Delete、Lane Action、业务菜单命令共用 ExecuteCommand。选择和展开状态按稳定 ID 协调；被拒绝的 Delete 保留选择。
- 可选事务只覆盖 Commit；参考 UObject 验证真实 UE Undo/Redo。NoChange/Reject 不产生多余撤销步骤。
- 回调重入不会重复提交或重复结束 Preview。开始回调取消时不会遗留 Mouse Capture；宿主销毁会解绑被外部留存的子 Widget。
- 可选 Sequencer/Viewport 路径接入时间同步与终止钩子；具体消费者仍负责业务 Context 和外部 Preview 目的地。

## 构建环境

| 项目 | 结果 |
| --- | --- |
| UE | 5.8.1，CL 56057345 |
| 目标 | 独立 WeaverReferenceEditor，Win64 Development |
| 编译工具 | 引擎自带 .NET 10 / UBT，VS 14.44，-NoUBA |
| 参考工程 | F:/codex/_WeaverReferenceValidation/WeaverReference.uproject |
| 最终构建 | Build6.log，Result: Succeeded |
| Core 校验 | 14/14 文件 SHA-256 与母版一致 |
| 静态检查 | git diff --check、UTF-8 严格读取、PowerShell 语法解析、uplugin JSON 解析通过 |

Core 与参考插件完全在同一个独立 Editor 模块中编译。没有 Runtime 模块反向依赖 Editor 的路径，没有引入 CAK/CineWeaver/AudioHead 类型。

## UE Automation 最终结果

运行参数为 -unattended -NullRHI，筛选 WeaverTimeline。最终报告记录 UTC 2026.09.17-16.10.40，对应北京时间 2026-09-18 00:10:40。

| 测试 | 结果 |
| --- | --- |
| Lifecycle.AuthorityWins | PASS |
| Lifecycle.CallbackBoundaries | PASS |
| Lifecycle.CommandsSelectionReentrancy | PASS |
| Lifecycle.EditKindsCascadeUndo | PASS |
| Lifecycle.Termination | PASS |
| Slate.PointerCommitCancel | PASS |

共 6 组，成功 6、带警告成功 0、失败 0、未运行 0。各组包含多个状态断言和策略分支，详细矩阵见 [Reference README](../Reference/README.md)。

PointerCommitCancel 从实际 Widget 树生成包含全部父级和虚拟指针位置的 FWidgetPath，路由 PointerDown/Move/Up 和 Capture；验证 Move、ResizeStart/End、Key、TimingRow 的权威值与刷新后的 Widget 数据。没有用直接调用 Commit 来替代这些输入步骤。TimingRow 的浮点几何结果使用 UE TestEqual 的标准浮点容差。

生命周期测试另外覆盖吸附 Proposal≠Authority、Clamp、Reject、多对象级联、重建 Widget、真正的 GEditor Undo/Redo、预览清理、稳定 ID、外部变更、上下文替换、重入和销毁。

## 本机证据

- [最终构建日志](F:/codex/_WeaverReferenceValidation/Build6.log)
- [最终自动化报告](F:/codex/_WeaverReferenceValidation/Report5/index.json)
- [最终自动化日志](F:/codex/_WeaverReferenceValidation/Automation5.log)
- [Core 哈希校验](F:/codex/_WeaverReferenceValidation/FinalCoreVerify.txt)

最终测试 PID 22368 已退出，日志含 TestExit Queue Empty 和退出状态 0；结束时没有残留 UnrealEditor/UnrealEditor-Cmd。早期失败报告保留在独立测试目录，未把它们计作 PASS；最终结果以上述 Report5 为准。

## 验证边界

本次完成的是母版实现、真实 UE 编译和 UE 内部自动化。NullRHI 的 Slate 合成事件测试不等于人工鼠标视觉验收，不检查屏幕像素、高 DPI、Sequencer 多窗口操作和真实角色/相机预览。正式消费者仍需实现 V5 Adapter、事务与源通知，并分别验收。

参考文档是 transactional 内存 UObject，支持 Widget 重建与 Undo/Redo；没有实现资产磁盘保存/重新加载。Core 无法检测既不更新 Revision 也不广播通知的任意业务写入；自定义父容器隐藏时须调用 Deactivate。

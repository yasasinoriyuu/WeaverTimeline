# WeaverTimeline 参考消费者

这是独立的 UE Editor 测试插件，用于证明源码母版的编辑闭环。权威对象是带 RF_Transactional 的 UWeaverReferenceDocument，临时 Preview 单独按 SessionId 保存。没有 CAK/CineWeaver/AudioHead 依赖。

## 准备与运行

先关闭 UE 编辑器，在仓库外选择 ASCII 测试目录：

```powershell
& .\tools\Prepare-ReferenceProject.ps1 -OutputRoot F:\codex\_WeaverReferenceValidation
```

脚本创建一个最小 Editor host，把 Reference 插件与全部 Core 复制过去并校验 SHA-256。拒绝覆盖没有专用标记的已有目录；不删除目标内容，不涉及 E 盘工程或正式消费者。生成的 Binaries、Intermediate、Saved 和日志只留在测试目录。

使用本机 UE5.8.1 的 UnrealBuildTool 构建 WeaverReferenceEditor，配置 Win64 Development，传入生成的 WeaverReference.uproject 和 -NoUBA。此处是独立参考工程目标，不使用生产工程 Project2BuildEditor，也不修改其配置。

打开生成的工程后，从“窗口”菜单打开“ Weaver 编排参考”。拖动片段、边缘、关键帧；展开片段后拖动生效区间 Handle。Delete 删除，Lane 的“新增”创建片段。提供普通、十帧吸附、范围限制、拒绝、级联推后五种策略，以及撤销、重做、切换数据源按钮。底部显示当前权威值和活动 Preview 数量，不以 Finished 回调数值宣布成功。

## 自动化测试

在 Session Frontend 的 Automation 中筛选 WeaverTimeline，或通过独立编辑器运行：

```text
UnrealEditor.exe <WeaverReference.uproject>
  -unattended -NullRHI -nosound -nosplash -NoLiveCoding
  -ExecCmds="Automation RunTests WeaverTimeline"
  -TestExit="Automation Test Queue Empty"
  -ReportExportPath=<独立报告目录>
  -abslog=<独立日志路径>
```

以上是多行展示的同一个命令。应记录本次 PID，等待自动退出，确认未残留本次编辑器。不要在用户已有编辑器旁启动额外实例。

| 测试 | 保护的行为 |
| --- | --- |
| Lifecycle.AuthorityWins | 普通、Snap、Clamp、Reject；权威值、Widget 回读、稳定 ID、重建 Widget、Preview 清理 |
| Lifecycle.EditKindsCascadeUndo | ResizeStart/End、Key、TimingRow、级联多对象刷新、真实 UE Undo/Redo、Revision 与通知 |
| Lifecycle.Termination | Cancel、Detach、隐藏、数据源切换、外部变更；旧 MouseUp 不提交，取消不覆盖新权威值 |
| Lifecycle.CommandsSelectionReentrancy | Create/Delete、选择和展开保留、删除清选择、Finished 回调重入 |
| Lifecycle.CallbackBoundaries | NoChange/Reject 不占额外 Undo 步骤、Delete 键拒绝/接受、Begin 回调换源、取消不遗留 Capture、宿主销毁、留存子 Widget 的解绑 |
| Slate.PointerCommitCancel | Slate 合成 PointerDown/Move/Up 与 Capture；Move、ResizeStart/End、Key、TimingRow 提交后权威回读、Esc、CaptureLost |
| Boundaries.CancelBeforeCommit | 创建事务时取消、隐藏、解绑或来源变化均不提交；事务和 Preview 仅清理一次，保留原因 |
| Boundaries.ScrubCancellation | Scrub 回调隐藏/换源、选择回调取消后不再捕获鼠标 |
| Boundaries.EndpointNotification | 左右端点的 Lane/Block/方向身份、可选播放头跳转、普通 Scrub 不误发事件 |
| Boundaries.SequencerOptIn | 真实 UE Sequencer 对象：默认禁用不受 Tick/Scrub/Pan/Zoom 影响；启用、解绑、重新启用 |

这些测试使用实际 UE UObject、FScopedTransaction、Slate Widget 与事件路由，断言权威状态和重新发布的 Widget 数据。NullRHI 测试不验证屏幕像素、物理鼠标或业务插件的实时角色/相机效果。

## 验证记录

当前构建/测试记录见 [VALIDATION_V6.md](../Docs/VALIDATION_V6.md)，历史记录见 [VALIDATION_V5.md](../Docs/VALIDATION_V5.md)。当前共 10 项测试。验收不能只看进程返回码：须检查找到的测试数量、每项结果、报告失败数与进程退出。

## 尚需人工或消费者验收

- 真实鼠标下绘制、命中位置、Timing Row 展开布局和高 DPI。
- Sequencer 多窗口、播放、打开/关闭及 focused Sequence 切换，Viewport 重挂载与几何对齐。
- CAK/CineWeaver/AudioHead 的实际 Adapter、业务对象事务和外部预览目的地。

参考插件为内存文档，不包含资产编辑器和磁盘保存/重载功能；Widget 重建持久性与 UObject Undo/Redo 已由测试覆盖，不能据此宣称磁盘持久化已验证。

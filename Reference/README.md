# WeaverTimeline V4 Reference Adapter

This directory is a reference consumer, not part of the byte-for-byte `Core/` copy.

It exists to preserve the complete CAK-proven edit loop in a business-neutral form:

```text
MouseDown
  -> Begin
MouseMove
  -> Timeline-owned visual preview
MouseUp
  -> Adapter Commit
  -> rebuild authoritative presentation
  -> Timeline SetLanes / SetKeys / SetBlocks
Esc / CaptureLost
  -> Adapter Cancel
  -> rebuild authoritative presentation
```

The important rule is that a consumer does not treat `On*Finished` as proof that an edit persisted. The adapter must mutate its authoritative source, and `FWeaverTimelineEditController` immediately rebuilds presentation data from that source. If an adapter returns `Accepted` but the rebuilt Key / Block / Timing Row does not match the requested final value, the controller records an invariant error and emits an `ensureMsgf` diagnostic.

## Minimal integration

```cpp
TSharedRef<FWeaverTimelineReferenceAdapter> Adapter =
    MakeShared<FWeaverTimelineReferenceAdapter>();

TSharedRef<FWeaverTimelineEditController> Controller =
    MakeShared<FWeaverTimelineEditController>(Adapter);

SNew(SWeaverEditableTimeline)
    .EditController(Controller)
    .CurrentFrame_Lambda([this]() { return CurrentFrame; });
```

Use `SWeaverEditableTimeline` as the default editable consumer path. Use the lower-level `SWeaverTimeline` directly only when the consumer intentionally owns the entire edit lifecycle itself.

## Required regression checks

- Block Move persists after MouseUp.
- ResizeStart persists after MouseUp.
- ResizeEnd persists after MouseUp.
- Key drag persists after MouseUp.
- Timing StartRatio / EndRatio persist after MouseUp.
- Escape restores authoritative pre-gesture state.
- CaptureLost restores authoritative pre-gesture state.
- `RejectNextCommit()` causes an intentional snap-back because the authoritative source rejected the proposal.
- An adapter must never return `Accepted` without actually persisting the requested final state; the controller detects this contract violation.

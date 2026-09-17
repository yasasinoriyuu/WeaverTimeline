# WeaverTimeline integration contract

## Purpose

`Core/` is reusable UE5 Editor presentation + interaction infrastructure only. It is intentionally copied into each consuming Editor Module instead of being loaded as a shared plugin dependency.

The source master now has four reusable layers:

```text
SWeaverTimeline
    self-drawn timeline interaction core

SWeaverEditableTimeline + FWeaverTimelineEditController
    CAK-proven generic edit-session closure

SWeaverTimelineHost + FWeaverViewportOverlay
    optional CAK-style bottom-of-Level-Viewport presentation shell

FWeaverSequencerBridge
    optional active-Sequencer time / view-range / geometry synchronization
```

A consumer may use only the layers it needs, but copied `Core/` source must remain byte-for-byte identical to the source master.

## Default editable integration path

Version 4 changes the recommended integration path for editable consumers.

Do **not** make every consumer manually rebuild the same lifecycle:

```text
On*Started
  -> On*Changed
  -> On*Finished
  -> write authoritative data
  -> rebuild FWeaver presentation
  -> SetBlocks / SetKeys
```

Instead, editable consumers should normally provide an `IWeaverTimelineEditAdapter`, wrap it in `FWeaverTimelineEditController`, and host the timeline through `SWeaverEditableTimeline`.

```text
business authoritative source
        ↑ commit / cancel
IWeaverTimelineEditAdapter
        ↑
FWeaverTimelineEditController
        ↑
SWeaverEditableTimeline
        ↑
SWeaverTimeline
```

This preserves the CAK-proven pattern:

```text
Begin
  -> visual Preview inside the timeline
  -> Commit on MouseUp OR Cancel on Esc/CaptureLost
  -> rebuild from authoritative source
  -> redraw authoritative result
```

The low-level `SWeaverTimeline` delegates remain available for advanced consumers that intentionally own the complete lifecycle themselves, but that is no longer the default path.

## Business adapter responsibilities

A consuming plugin Adapter must:

1. Build stable `FGuid` identities for lanes, keys and blocks.
2. Build `FWeaverTimelinePresentation` from its authoritative business data.
3. Own persistence, transactions, Undo/Redo and runtime meaning.
4. Implement commit functions for the edit types it supports.
5. Return `Accepted` only after the requested final value has actually been written to its authoritative source.
6. Return `Rejected` when business rules refuse the proposal.
7. Keep preview callbacks transient; do not silently turn them into persistent writes.
8. Own the business meaning of the current frame and respond to incoming Sequencer frame changes.

The Core never writes MovieScene business data, assets, camera state, action data or audio data itself.

## Commit invariant

`FWeaverTimelineEditController` always rebuilds presentation data after Commit or Cancel.

If an Adapter returns `Accepted`, the rebuilt authoritative presentation must contain the same final value requested by the edit gesture:

- Key: final frame
- Block: final start/end
- Timing Row: final start/end ratio

If the authoritative rebuild does not match, the Controller stores a validation error and emits `ensureMsgf`. This catches the exact class of bug where a UI says an edit succeeded but the consumer forgot to persist it.

A visible snap-back therefore has only two valid meanings under the default V4 path:

- the Adapter returned `Rejected`, so authoritative business rules intentionally refused the edit; or
- the Adapter violated the Accepted contract, which is diagnosed as an invariant failure.

It must not be silently treated as success.

## Preview rule

`SWeaverTimeline` owns visual drag preview state for Key / Block / Timing Row gestures. Consumers do not need to call `SetBlocks()` or `SetKeys()` on every mouse move.

Optional Adapter preview callbacks exist only for external transient effects such as runtime preview. They are not authoritative persistence.

This mirrors CAK's proven `BeginMoveBlock -> PreviewMoveBlock -> CommitMoveBlock` pattern: high-frequency pointer motion does not repeatedly write persistent assets.

## Cancel rule

Escape and mouse-capture loss are cancellation paths.

The Controller calls the Adapter's matching cancel callback and then rebuilds presentation from the authoritative source. The Adapter may use cancel callbacks to undo transient external preview effects, but it should not need to reconstruct authoritative state if preview never wrote persistent data.

## UI-only expansion state

Block expansion is presentation state, not business persistence.

`FWeaverTimelineEditController` remembers expanded `BlockId`s across authoritative presentation rebuilds. A consumer does not need to serialize `bExpanded` into MovieScene, CAK assets, audio data, or other business storage.

## Stable identity rule

Do not use array position as identity. `LaneId`, `KeyId` and `BlockId` must remain stable while the corresponding business item exists. Array ordering is display ordering only.

## Drag lifecycle

Low-level Key editing emits:

```text
OnKeyEditStarted
  -> zero or more OnKeyEditChanged
  -> OnKeyEditFinished(..., bCancelled)
```

Low-level Block editing emits the same lifecycle with `EWeaverBlockEditKind` identifying Move / ResizeStart / ResizeEnd.

Block endpoint interaction has a small pending phase. A left click on a block endpoint that stays below the drag threshold emits `OnBlockEndpointClicked(LaneId, BlockId, bStart)` and does not start a resize lifecycle. Once the pointer crosses the threshold, the normal block resize lifecycle begins.

Timing rows use the same Started -> Changed -> Finished lifecycle as other edits.

When `SWeaverEditableTimeline` is used, these low-level lifecycle delegates are wired to `FWeaverTimelineEditController`; consumers implement the Adapter instead of manually reconstructing the lifecycle.

## External ViewRange mode

`SetExternalViewRange(Start, End)` means another system owns the authoritative visible time range.

In this mode, RMB pan and wheel zoom emit `OnViewRangeChanged(NewStart, NewEnd)`, but the widget does not make that new range authoritative by itself.

When `FWeaverSequencerBridge` is used, bind the timeline's view-range request to `PushTimelineViewRange`. The bridge applies the request to Sequencer; the next `Sync()` mirrors Sequencer's resulting range back into the timeline.

Without external mode, the widget updates its own local range and also emits the delegate.

## Sequencer bridge contract

`FWeaverSequencerBridge` is editor infrastructure, not business logic.

The consumer should:

- register the bridge with its `SWeaverTimeline` instance;
- provide the timeline display-rate resolver when its frame domain is not simply Sequencer's focused display rate;
- receive `FOnWeaverSequencerFrameChanged` and update its own current-frame/business preview state;
- bind timeline scrub changes to `PushTimelineFrame`;
- bind timeline view-range changes to `PushTimelineViewRange`;
- call `Sync(TimelineGeometry)` from the owning Slate widget's `Tick`.

`Sync()` mirrors Sequencer's view range, aligns the self-drawn track area with Sequencer's `TrackAreaView`, and pulls SubFrame time during playback. Active-Sequencer discovery is intentionally low-frequency, following CAK's proven pattern rather than scanning the editor every frame.

## Viewport host / overlay contract

`SWeaverTimelineHost` only owns collapse/expand, panel height and resize mouse-capture behavior.

`FWeaverViewportOverlay` only owns attachment to the active Level Editor viewport. It uses the same bottom-aligned overlay placement proven in CAK and rechecks the active viewport once per second.

`SetVisible(false)` collapses the overlay root and removes it from hit testing while leaving registration, timeline data and all business/evaluation state intact.

Neither layer may create business panels or reference Camera / CharacterAction / Audio systems directly.

## Context menu rule

Core distinguishes RMB click from RMB drag:

- RMB drag: horizontal timeline pan.
- RMB click on Key/Block: `OnContextRequested`.
- RMB click on an empty track area: `OnLaneContextRequested(LaneId, Frame, ScreenPosition)`.

Core does not build or register business menus. The Adapter owns any menu UI.

Lane header actions are presentation data in `FWeaverLane::HeaderActions`. An enabled action click emits `OnLaneHeaderActionRequested(LaneId, ActionId)`; the Adapter or owning Panel owns the action's business meaning and toggled-state update.

## Delete rule

Delete/Backspace emits `OnDeleteRequested`. The Core clears visual selection but does not delete business data itself. Delete remains a business operation because ownership, transactions and validation differ by consumer.

## Reference adapter

`Reference/WeaverTimelineReferenceAdapter.*` is the canonical minimal consumer example. It is intentionally not part of copied `Core/`.

It must keep the following regression checks demonstrable:

- Block Move persists after MouseUp.
- ResizeStart persists after MouseUp.
- ResizeEnd persists after MouseUp.
- Key drag persists after MouseUp.
- Timing Start/End ratios persist after MouseUp.
- Esc and CaptureLost restore authoritative pre-gesture state.
- Commit rejection intentionally restores authoritative state.
- Returning `Accepted` without persistence is diagnosed as a contract violation.

New consumers should compare their Adapter against this reference before inventing their own edit lifecycle.

## Module requirements

Base Timeline / EditableTimeline / Host use standard dependencies:

```text
Core
Slate
SlateCore
InputCore
```

Viewport Overlay and Sequencer Bridge require additional UE Editor modules. CAK's currently working implementation includes:

```text
UnrealEd
LevelEditor
MovieScene
Sequencer
SequencerWidgets
```

Do not treat that list as a frozen implementation recipe across UE versions. The consuming plugin's local UE5 Skill and compile result are authoritative.

No module API macro is used by the source master. Keep the copied Core inside the consuming module rather than exporting it across module boundaries.

## Forbidden additions to Core

Do not add business-specific knowledge such as:

```text
Camera
StateKey
Orbit
CharacterAction
Pose
ControlRig
Audio
HeadMotion
MovieScene business persistence
plugin-specific Tab / ToolMenu / StyleSet registration
```

`FWeaverTimelineEditController` may orchestrate generic edit lifecycle, but it must never know what a Camera Motion Block, Character Action, audio gesture, or MovieScene Section means.

If a new feature only makes sense for one consumer, implement it in that consumer's Adapter first. Promote it into Core only when it is genuinely interaction infrastructure shared by the editors.

# WeaverTimeline integration contract

## Purpose

`Core/` is reusable UE5 Editor presentation + interaction infrastructure only. It is intentionally copied into each consuming Editor Module instead of being loaded as a shared plugin dependency.

The source master has three reusable layers:

```text
SWeaverTimeline
    self-drawn timeline interaction core

SWeaverTimelineHost + FWeaverViewportOverlay
    optional CAK-style bottom-of-Level-Viewport presentation shell

FWeaverSequencerBridge
    optional active-Sequencer time / view-range / geometry synchronization
```

A consumer may use only the layers it needs, but copied Core source must remain byte-for-byte identical to the source master.

## Business adapter responsibilities

A consuming plugin must:

1. Build stable `FGuid` identities for lanes, keys and blocks.
2. Map its business data into `FWeaverLane`, `FWeaverKey`, `FWeaverBlock`.
3. Own persistence, transactions, Undo/Redo and runtime meaning.
4. Apply edit delegates back to business data.
5. Re-feed updated view data to `SWeaverTimeline` after edits.
6. Own the business meaning of the current frame and respond to incoming Sequencer frame changes.

The Core never writes MovieScene business data, assets, camera state, action data or audio data itself.

## Stable identity rule

Do not use array position as identity. `LaneId`, `KeyId` and `BlockId` must remain stable while the corresponding business item exists. Array ordering is display ordering only.

## Drag lifecycle

Key editing emits:

```text
OnKeyEditStarted
  -> zero or more OnKeyEditChanged
  -> OnKeyEditFinished(..., bCancelled)
```

Block editing emits the same lifecycle with `EWeaverBlockEditKind` identifying Move / ResizeStart / ResizeEnd.

Block endpoint interaction has a small pending phase. A left click on a block endpoint
that stays below the drag threshold emits `OnBlockEndpointClicked(LaneId, BlockId,
bStart)` and does not start a resize lifecycle. Once the pointer crosses the threshold,
the normal block resize lifecycle begins.

Timing rows use the same Started -> Changed -> Finished lifecycle as other edits. The
adapter owns the meaning of `FWeaverTimingRow::RowId` and must restore its original
ratios when `bCancelled == true`.

The adapter should normally start its transaction or preview session on `Started`, update a preview or business candidate on `Changed`, and commit or revert on `Finished`.

If `bCancelled == true`, the adapter must restore its original business state. This is used for Escape and unexpected mouse-capture loss.

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

`SetVisible(false)` collapses the overlay root and removes it from hit testing while
leaving registration, timeline data and all business/evaluation state intact.

Neither layer may create business panels or reference Camera / CharacterAction / Audio systems directly.

## Context menu rule

Core distinguishes RMB click from RMB drag:

- RMB drag: horizontal timeline pan.
- RMB click on Key/Block: `OnContextRequested`.
- RMB click on an empty track area: `OnLaneContextRequested(LaneId, Frame, ScreenPosition)`.

Core does not build or register business menus. The adapter owns any menu UI.

Lane header actions are presentation data in `FWeaverLane::HeaderActions`. An enabled
action click emits `OnLaneHeaderActionRequested(LaneId, ActionId)`; the adapter owns
the action's business meaning and toggled-state update.

## Delete rule

Delete/Backspace emits `OnDeleteRequested`. The Core clears visual selection but does not delete business data itself.

## Module requirements

Base Timeline / Host use standard dependencies:

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

If a new feature only makes sense for one consumer, implement it in that consumer's Adapter first. Promote it into Core only when it is genuinely interaction infrastructure shared by the editors.

# WeaverTimeline integration contract

## Purpose

`Core/` is presentation + interaction infrastructure only. It is intentionally copied into each consuming Editor Module instead of being loaded as a shared plugin dependency.

## Business adapter responsibilities

A consuming plugin must:

1. Build stable `FGuid` identities for lanes, keys and blocks.
2. Map its business data into `FWeaverLane`, `FWeaverKey`, `FWeaverBlock`.
3. Own persistence, transactions, Undo/Redo and runtime meaning.
4. Apply edit delegates back to business data.
5. Re-feed updated view data to `SWeaverTimeline` after edits.
6. Own Sequencer binding and time/view-range synchronization if used.

The Core never writes MovieScene, assets, camera data, action data or audio data itself.

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

The adapter should normally start its transaction or preview session on `Started`, update a preview or business candidate on `Changed`, and commit or revert on `Finished`.

If `bCancelled == true`, the adapter must restore its original business state. This is used for Escape and unexpected mouse-capture loss.

## External ViewRange mode

`SetExternalViewRange(Start, End)` means another system (normally Sequencer) owns the authoritative visible time range.

In this mode, RMB pan and wheel zoom still emit `OnViewRangeChanged(NewStart, NewEnd)`, but the widget does not make that new range authoritative by itself. The adapter should apply it to the owner and then call `SetExternalViewRange` with the resulting range.

Without external mode, the widget updates its own local range and also emits the delegate.

## Context menu rule

Core distinguishes RMB click from RMB drag:

- RMB drag: horizontal timeline pan.
- RMB click on Key/Block: `OnContextRequested`.

Core does not build or register business menus. The adapter owns any menu UI.

## Delete rule

Delete/Backspace emits `OnDeleteRequested`. The Core clears visual selection but does not delete business data itself.

## Module requirements

A consumer Editor Module needs these standard dependencies:

```text
Core
Slate
SlateCore
InputCore
```

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
MovieScene persistence
plugin-specific Tab / ToolMenu / StyleSet registration
```

If a new feature only makes sense for one consumer, implement it in that consumer's Adapter first. Promote it into Core only when it is genuinely interaction infrastructure shared by the editors.

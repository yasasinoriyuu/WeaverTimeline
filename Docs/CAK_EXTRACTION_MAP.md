# CAK extraction map

This document records which reusable editor behaviors were extracted from CharacterActionKit and which CAK-specific behaviors intentionally remain outside WeaverTimeline.

## Source authority

CAK source branch used as behavior reference:

```text
CharacterActionKit
branch: refactor/self-drawn-action-planner
```

Relevant proven files:

```text
Source/CharacterActionKitPlannerEditor/Private/SCharacterActionPlannerTimeline.h/.cpp
Source/CharacterActionKitPlannerEditor/Public/CharacterActionPlannerModel.h
Source/CharacterActionKitPlannerEditor/Private/CharacterActionPlannerModel.cpp
Source/CharacterActionKitPlannerEditor/Private/SCharacterActionPlannerViewportHost.h/.cpp
Source/CharacterActionKitPlannerEditor/Private/CharacterActionPlannerViewportOverlay.h/.cpp
Source/CharacterActionKitPlannerEditor/Private/SCharacterActionPlannerPanel.h/.cpp
```

## What CAK actually proved

CAK's stable block move is not only a Slate gesture. It is a complete edit loop:

```text
MouseDown
  -> FCharacterActionPlannerModel::BeginMoveBlock
MouseMove
  -> PreviewMoveBlock
  -> timeline paints PreviewStartFrame
MouseUp
  -> CommitMoveBlock
  -> authoritative Arrangement changes
  -> drag preview resets
  -> next paint reads the committed Arrangement value
CaptureLost
  -> preview state resets without committing
```

`CharacterActionPlannerModel.h` explicitly states that persistent mutation happens on MouseUp, while pointer movement uses preview state instead of high-frequency asset writes.

This lifecycle is the behavior reference for WeaverTimeline Version 4.

## Extracted into WeaverTimeline

### Timeline interaction core

From `SCharacterActionPlannerTimeline` and the CineWeaver neutralization passes:

- self-drawn Slate timeline
- ruler and playhead
- frame <-> local X mapping
- track/lane layout
- hit testing
- selection and hover
- key drag lifecycle
- block move lifecycle
- block edge resize lifecycle
- endpoint click vs drag threshold
- expandable timing rows and ratio handles
- scrub
- horizontal pan
- wheel zoom
- context-click versus RMB-drag distinction
- keyboard delete request
- mouse capture cleanup and cancellation
- external visible-range mode

The source master generalizes identity to stable `LaneId / KeyId / BlockId` instead of treating array index as business identity.

### Generic edit-session closure (Version 4)

Version 3 stopped at generic drag delegates. That was too thin: every consumer still had to remember to translate `Finished` into authoritative persistence and then re-feed the timeline.

Version 4 extracts the reusable part of CAK's `Begin -> Preview -> Commit/Cancel -> authoritative redraw` pattern into:

```text
FWeaverTimelineEditController
SWeaverEditableTimeline
IWeaverTimelineEditAdapter
```

The Controller is business-neutral. It does not know about Arrangement, MovieScene, Camera, Action, Audio, UObject transactions, overlap rules, or runtime evaluation.

It does guarantee the generic lifecycle:

```text
Begin
  -> optional external transient preview
  -> Commit or Cancel
  -> rebuild authoritative presentation
  -> re-feed SWeaverTimeline
```

It also verifies the central invariant: if an Adapter returns `Accepted`, the rebuilt authoritative Key / Block / Timing Row must match the requested final value. This prevents a consumer from reporting success while the UI silently snaps back because nothing was persisted.

### Viewport host

From `SCharacterActionPlannerViewportHost`:

- bottom panel height
- 7 px top resize zone
- expanded-height drag behavior
- collapse / expand behavior
- mouse capture cleanup
- CAK-proven height range behavior

The extracted `SWeaverTimelineHost` accepts arbitrary Slate content and contains no CAK panel construction.

### Active Level Viewport overlay

From `FCharacterActionPlannerViewportOverlay`:

- attach content to the bottom of the active Level Editor viewport
- use Level Editor active viewport discovery
- detach when the active viewport changes
- low-frequency 1-second discovery ticker
- overlay priority behavior

The extracted `FWeaverViewportOverlay` accepts arbitrary content and has no CharacterAction dependency.

### Sequencer synchronization

From `SCharacterActionPlannerPanel`:

- discover an active Level Editor Sequencer through `FLevelEditorSequencerIntegration`
- subscribe/unsubscribe `OnGlobalTimeChanged`
- convert timeline display frames to Sequencer focused tick resolution
- convert Sequencer local time back to timeline display frames
- mirror Sequencer `ViewRange`
- push timeline view-range requests through `SetViewRange(... Immediate)`
- find `TrackAreaView` recursively from the Sequencer widget
- align self-drawn track area horizontally to Sequencer track geometry
- during playback, pull local SubFrame time from Slate Tick because global-time events can be coarser in some settings

These behaviors live in `FWeaverSequencerBridge` and carry no CAK actor/action logic.

## Intentionally not extracted

The following remain CAK business behavior and should not move into WeaverTimeline:

- `UCharacterActionArrangementAsset`
- Action / Pose / DerivedBoneData ownership
- action-block conflict rules
- repeat-count semantics and repeat handles
- mouth-track samples / mouth snapshot logic
- action display-name resolution
- Strength / FrameInterval menus
- actor selection and binding
- playback component preview state
- CharacterAction runtime evaluation
- CAK asset creation and persistence
- CAK quick action palette
- CAK-specific `FScopedTransaction` text and `Arrangement.Modify()` calls

For other consumers, their Adapter owns the corresponding business persistence and transaction logic.

## Reference consumer

`Reference/WeaverTimelineReferenceAdapter.*` is the canonical pure-memory consumer for Version 4. It exists so future adapters can compare against one known-good implementation instead of re-deriving the edit contract from prose.

## Current extraction status

Source Version 4 contains:

```text
self-drawn timeline             extracted
edit-session closure            extracted
reference adapter               provided
viewport host                   extracted
active viewport overlay         extracted
sequencer time sync             extracted
sequencer view sync             extracted
sequencer track alignment       extracted
```

The next gate is consumer integration and UE5.8 compilation: sync the whole Version 4 `Core/` into CineWeaver, wire a business Adapter through `SWeaverEditableTimeline`, and validate the same mouse gestures in the real UE project.

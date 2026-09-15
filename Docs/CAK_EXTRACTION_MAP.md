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
Source/CharacterActionKitPlannerEditor/Private/SCharacterActionPlannerViewportHost.h/.cpp
Source/CharacterActionKitPlannerEditor/Private/CharacterActionPlannerViewportOverlay.h/.cpp
Source/CharacterActionKitPlannerEditor/Private/SCharacterActionPlannerPanel.h/.cpp
```

## Extracted into WeaverTimeline

### Timeline interaction core

From `SCharacterActionPlannerTimeline` and the first CineWeaver neutralization pass:

- self-drawn Slate timeline
- ruler and playhead
- frame <-> local X mapping
- track/lane layout
- hit testing
- selection and hover
- key drag lifecycle
- block move lifecycle
- block edge resize lifecycle
- scrub
- horizontal pan
- wheel zoom
- context-click versus RMB-drag distinction
- keyboard delete request
- mouse capture cleanup and cancellation
- external visible-range mode

The source master generalizes identity to stable `LaneId / KeyId / BlockId` instead of treating array index as business identity.

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

If another plugin later needs a visually similar feature, it should first be implemented in that plugin's Adapter. Only behavior that is genuinely business-neutral should be promoted into the source master.

## Current extraction status

Source Version 2 contains the reusable parts of CAK's complete editor shell:

```text
self-drawn timeline        extracted
viewport host              extracted
active viewport overlay    extracted
sequencer time sync        extracted
sequencer view sync        extracted
sequencer track alignment  extracted
```

The remaining work is not more CAK business extraction. The next meaningful gate is consumer integration and UE compilation: copy Version 2 into a real consumer Editor Module, wire its Adapter, follow the local UE5 Skill, and compile-fix against the actual UE5.8 environment.

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

## Extracted into WeaverTimeline

### Editing lifecycle (V5)

The V4 controller introduced mandatory authority refresh. V5 retains that direction and completes the session contract; it replaces the V4 rule that an Accepted result must equal the pointer proposal.

| CAK collaboration | Business-neutral V5 responsibility |
| --- | --- |
| Timeline reads Arrangement authority on paint | Adapter builds full authoritative presentation after every terminal path |
| PlannerModel BeginMove / PreviewMove / CommitMove | Session keeps Original and Proposal separate until commit |
| PlannerModel conflict resolution and following-block adjustment | Applied may resolve a different value or change multiple objects; authority wins |
| Short FScopedTransaction and Arrangement.Modify | Consumer supplies a commit-only transaction scope and its own authoritative objects |
| Panel forwards transient preview to PlaybackComponent; AnimNode applies it to a cached track | BeginPreview / UpdatePreview / EndPreview extension points, with cleanup owned by the controller |
| Panel notifies durable changes to playback and cached evaluation | Source notifications and post-reconciliation notification, including consumer Undo/Redo wiring |

Arrangement, PlaybackComponent and AnimNode belong to the complete CAK system being studied, but their concrete data and algorithms stay out of Core. V5 Session identity, stale-source cancellation, command routing and teardown guarantees are explicit generic infrastructure with their own reference tests; they are not claims that CAK already implemented every edge case.

The runnable transactional consumer now lives under `Reference/Source/WeaverTimelineReference`. It replaces the V4 plain-memory `Reference/WeaverTimelineReferenceAdapter.*` draft. See `INTEGRATION_CONTRACT.md` for migration and `VALIDATION_V5.md` for measured results.

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
- concrete playback component fields and runtime evaluation; generic preview begin/update/end lifecycle belongs in Core
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

The list above describes the historical V2 shell, not a complete editing lifecycle. CAK paints directly from Arrangement authority, whereas V3 held manually refreshed presentation copies. CAK's model resolves conflicts and may change additional blocks, while its panel/playback path separately ends transient previews and notifies durable changes.

V5 adds business-neutral Session/Context identity, proposal-based commit, full authority reconciliation, optional commit-only transaction scopes, external preview cleanup, source/Undo notifications and a mutation command path. The concrete Arrangement, conflict algorithm, actor and animation types stay in CAK. See INTEGRATION_CONTRACT.md and the transactional Reference consumer. CAK does not establish every generic interaction (for example Key/TimingRow editing and Esc); new guarantees require their own regression tests.

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "Misc/FrameRate.h"
#include "MovieSceneSequenceID.h"

class ISequencer;
class SWidget;
class SWeaverTimeline;
class UMovieSceneSequence;

DECLARE_DELEGATE_OneParam(FOnWeaverSequencerFrameChanged, double)

/**
 * Optional editor-side bridge between SWeaverTimeline and the active Level Editor Sequencer.
 *
 * Responsibilities:
 * - discover/rebind the active Sequencer using the same integration path proven in CAK;
 * - mirror Sequencer time and visible range into the self-drawn timeline;
 * - push user-authored frame/view-range changes back into Sequencer;
 * - align the self-drawn track area with Sequencer's TrackAreaView geometry.
 *
 * It does not own business data, persistence, runtime evaluation or Undo semantics.
 */
class FWeaverSequencerBridge
{
public:
    using FDisplayRateProvider = TFunction<FFrameRate()>;

    FWeaverSequencerBridge() = default;
    ~FWeaverSequencerBridge();

    void Register(
        const TSharedRef<SWeaverTimeline>& InTimeline,
        FDisplayRateProvider InDisplayRateProvider,
        FOnWeaverSequencerFrameChanged InOnSequencerFrameChanged,
        FSimpleDelegate InOnContextChanged = FSimpleDelegate());
    void Unregister();

    /** Native section hosts must bind their owner, never discover another editor window. */
    void RegisterForSequencer(
        const TSharedRef<SWeaverTimeline>& InTimeline,
        TWeakPtr<ISequencer> InSequencer,
        FDisplayRateProvider InDisplayRateProvider,
        FOnWeaverSequencerFrameChanged InOnSequencerFrameChanged,
        FSimpleDelegate InOnContextChanged = FSimpleDelegate(),
        bool bInEmbeddedLayout = false);

    /** Call from the owning Slate widget's Tick using the timeline widget geometry. */
    void Sync(const FGeometry& TimelineGeometry);

    /** Bind SWeaverTimeline::OnFrameChanged to this when Sequencer should follow timeline scrubbing. */
    void PushTimelineFrame(double NewFrame);

    /** Bind SWeaverTimeline::OnViewRangeChanged to this when Sequencer owns the visible time range. */
    void PushTimelineViewRange(double StartFrame, double EndFrame);

    bool IsBound() const { return Sequencer.IsValid(); }
    bool IsRegistered() const { return bRegistered; }

private:
    bool TickBinding(float DeltaTime);
    void RefreshBinding();
    void HandleSequencerTimeChanged();
    void DetachSequencer();
    void HandleSequencerClosed(TSharedRef<ISequencer> Closed);
    void HandleSequenceActivated(FMovieSceneSequenceIDRef);
    FFrameRate ResolveDisplayRate(const TSharedPtr<ISequencer>& ActiveSequencer) const;

    TWeakPtr<SWeaverTimeline> Timeline;
    TWeakPtr<ISequencer> Sequencer;
    TWeakPtr<SWidget> SequencerTrackAreaWidget;
    FDelegateHandle SequencerTimeChangedHandle;
    FDelegateHandle SequencerClosedHandle;
    FDelegateHandle SequenceActivatedHandle;
    TWeakPtr<ISequencer> ExplicitSequencer;
    bool bExplicitBinding = false;
    bool bEmbeddedLayout = false;
    FTSTicker::FDelegateHandle BindingTickerHandle;

    FDisplayRateProvider DisplayRateProvider;
    FOnWeaverSequencerFrameChanged OnSequencerFrameChanged;
    FSimpleDelegate OnContextChanged;
    TWeakObjectPtr<UMovieSceneSequence> FocusedSequence;
    FMovieSceneSequenceID FocusedTemplate;
    bool bNotifyingTime = false;
    bool bRegistered = false;

    bool bHasAppliedPadding = false;
    float LastAppliedLeftPadding = 0.0f;
    float LastAppliedRightPadding = 0.0f;
};

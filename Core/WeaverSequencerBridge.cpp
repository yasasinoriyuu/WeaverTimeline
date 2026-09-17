#include "WeaverSequencerBridge.h"

#include "AnimatedRange.h"
#include "ISequencer.h"
#include "LevelEditorSequencerIntegration.h"
#include "MovieSceneFwd.h"
#include "MovieSceneSequence.h"
#include "SWeaverTimeline.h"
#include "ViewRangeInterpolation.h"
#include "Widgets/SWidget.h"

namespace
{
TSharedPtr<SWidget> FindWeaverTrackAreaWidgetRecursive(const TSharedRef<SWidget>& Widget)
{
    if (Widget->GetTypeAsString().Contains(TEXT("TrackAreaView")))
    {
        return Widget;
    }

    FChildren* Children = Widget->GetChildren();
    if (!Children)
    {
        return nullptr;
    }

    for (int32 Index = 0; Index < Children->Num(); ++Index)
    {
        const TSharedRef<SWidget> Child = Children->GetChildAt(Index);
        if (TSharedPtr<SWidget> Found = FindWeaverTrackAreaWidgetRecursive(Child))
        {
            return Found;
        }
    }

    return nullptr;
}
}

FWeaverSequencerBridge::~FWeaverSequencerBridge()
{
    Unregister();
}

void FWeaverSequencerBridge::Register(
    const TSharedRef<SWeaverTimeline>& InTimeline,
    FDisplayRateProvider InDisplayRateProvider,
    FOnWeaverSequencerFrameChanged InOnSequencerFrameChanged,
    FSimpleDelegate InOnContextChanged)
{
    Unregister();

    Timeline = InTimeline;
    DisplayRateProvider = MoveTemp(InDisplayRateProvider);
    OnSequencerFrameChanged = MoveTemp(InOnSequencerFrameChanged);
    OnContextChanged = MoveTemp(InOnContextChanged);

    BindingTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FWeaverSequencerBridge::TickBinding),
        1.0f);

    RefreshBinding();
}

void FWeaverSequencerBridge::Unregister()
{
    if (BindingTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(BindingTickerHandle);
        BindingTickerHandle.Reset();
    }

    DetachSequencer();
    Timeline.Reset();
    DisplayRateProvider = FDisplayRateProvider();
    OnSequencerFrameChanged.Unbind();
    OnContextChanged.Unbind();
}

bool FWeaverSequencerBridge::TickBinding(float DeltaTime)
{
    RefreshBinding();
    return true;
}

void FWeaverSequencerBridge::RefreshBinding()
{
    TSharedPtr<ISequencer> Candidate;
    for (const TWeakPtr<ISequencer>& WeakSequencer :
        FLevelEditorSequencerIntegration::Get().GetSequencers())
    {
        if (WeakSequencer.IsValid())
        {
            Candidate = WeakSequencer.Pin();
            break;
        }
    }

    if (Candidate == Sequencer.Pin())
    {
        if (Candidate && FocusedSequence.Get() != Candidate->GetFocusedMovieSceneSequence())
        {
            FocusedSequence = Candidate->GetFocusedMovieSceneSequence();
            OnContextChanged.ExecuteIfBound();
        }
        return;
    }

    DetachSequencer();
    Sequencer = Candidate;

    if (Candidate.IsValid())
    {
        FocusedSequence = Candidate->GetFocusedMovieSceneSequence();
        OnContextChanged.ExecuteIfBound();
        SequencerTrackAreaWidget = FindWeaverTrackAreaWidgetRecursive(
            Candidate->GetSequencerWidget());
        SequencerTimeChangedHandle = Candidate->OnGlobalTimeChanged().AddRaw(
            this,
            &FWeaverSequencerBridge::HandleSequencerTimeChanged);
        HandleSequencerTimeChanged();
    }
}

void FWeaverSequencerBridge::DetachSequencer()
{
    const bool bHadBinding = SequencerTimeChangedHandle.IsValid();
    if (const TSharedPtr<ISequencer> Previous = Sequencer.Pin())
    {
        if (SequencerTimeChangedHandle.IsValid())
        {
            Previous->OnGlobalTimeChanged().Remove(SequencerTimeChangedHandle);
        }
    }

    SequencerTimeChangedHandle.Reset();
    SequencerTrackAreaWidget.Reset();
    Sequencer.Reset();
    FocusedSequence.Reset();
    if (bHadBinding) { OnContextChanged.ExecuteIfBound(); }

    if (const TSharedPtr<SWeaverTimeline> TimelineWidget = Timeline.Pin())
    {
        TimelineWidget->ClearExternalViewRange();
        if (bHasAppliedPadding)
        {
            TimelineWidget->SetHorizontalPadding(0.0f, 12.0f);
        }
    }

    bHasAppliedPadding = false;
    LastAppliedLeftPadding = 0.0f;
    LastAppliedRightPadding = 0.0f;
}

FFrameRate FWeaverSequencerBridge::ResolveDisplayRate(
    const TSharedPtr<ISequencer>& ActiveSequencer) const
{
    if (DisplayRateProvider)
    {
        return DisplayRateProvider();
    }

    return ActiveSequencer.IsValid()
        ? ActiveSequencer->GetFocusedDisplayRate()
        : FFrameRate(30, 1);
}

void FWeaverSequencerBridge::PushTimelineFrame(const double NewFrame)
{
    const TSharedPtr<ISequencer> ActiveSequencer = Sequencer.Pin();
    if (bNotifyingTime || !ActiveSequencer.IsValid() || !FMath::IsFinite(NewFrame))
    {
        return;
    }

    const FFrameRate TimelineRate = ResolveDisplayRate(ActiveSequencer);
    const FFrameTime TickTime = FFrameRate::TransformTime(
        FFrameTime::FromDecimal(NewFrame),
        TimelineRate,
        ActiveSequencer->GetFocusedTickResolution());

    ActiveSequencer->SetLocalTimeDirectly(TickTime, true);
}

void FWeaverSequencerBridge::PushTimelineViewRange(
    const double StartFrame,
    const double EndFrame)
{
    const TSharedPtr<ISequencer> ActiveSequencer = Sequencer.Pin();
    if (!ActiveSequencer.IsValid()
        || !FMath::IsFinite(StartFrame)
        || !FMath::IsFinite(EndFrame)
        || EndFrame <= StartFrame)
    {
        return;
    }

    const FFrameRate TimelineRate = ResolveDisplayRate(ActiveSequencer);
    const double FramesPerSecond = FMath::Max(TimelineRate.AsDecimal(), 0.001);

    ActiveSequencer->SetViewRange(
        TRange<double>(StartFrame / FramesPerSecond, EndFrame / FramesPerSecond),
        EViewRangeInterpolation::Immediate);
}

void FWeaverSequencerBridge::Sync(const FGeometry& TimelineGeometry)
{
    RefreshBinding(); // Detect sequence/window changes before each editable-frame tick.
    const TSharedPtr<ISequencer> ActiveSequencer = Sequencer.Pin();
    const TSharedPtr<SWeaverTimeline> TimelineWidget = Timeline.Pin();
    if (!ActiveSequencer.IsValid() || !TimelineWidget.IsValid())
    {
        if (TimelineWidget.IsValid())
        {
            TimelineWidget->ClearExternalViewRange();
        }
        return;
    }

    const FFrameRate TimelineRate = ResolveDisplayRate(ActiveSequencer);
    const double FramesPerSecond = FMath::Max(TimelineRate.AsDecimal(), 0.001);

    const FAnimatedRange ViewRange = ActiveSequencer->GetViewRange();
    if (ViewRange.HasLowerBound() && ViewRange.HasUpperBound())
    {
        TimelineWidget->SetExternalViewRange(
            ViewRange.GetLowerBoundValue() * FramesPerSecond,
            ViewRange.GetUpperBoundValue() * FramesPerSecond);
    }

    TSharedPtr<SWidget> TrackArea = SequencerTrackAreaWidget.Pin();
    if (!TrackArea.IsValid())
    {
        TrackArea = FindWeaverTrackAreaWidgetRecursive(ActiveSequencer->GetSequencerWidget());
        SequencerTrackAreaWidget = TrackArea;
    }

    if (TrackArea.IsValid())
    {
        const FGeometry& TrackGeometry = TrackArea->GetCachedGeometry();
        if (TrackGeometry.GetLocalSize().X > 1.0f && TimelineGeometry.GetLocalSize().X > 1.0f)
        {
            const float Left = FMath::Clamp(
                TimelineGeometry.AbsoluteToLocal(TrackGeometry.GetAbsolutePosition()).X,
                0.0f,
                TimelineGeometry.GetLocalSize().X);
            const float Right = FMath::Clamp(
                TimelineGeometry.AbsoluteToLocal(
                    TrackGeometry.LocalToAbsolute(TrackGeometry.GetLocalSize())).X,
                Left,
                TimelineGeometry.GetLocalSize().X);
            const float RightPadding = FMath::Max(
                0.0f,
                TimelineGeometry.GetLocalSize().X - Right);

            if (!bHasAppliedPadding
                || !FMath::IsNearlyEqual(LastAppliedLeftPadding, Left, 0.5f)
                || !FMath::IsNearlyEqual(LastAppliedRightPadding, RightPadding, 0.5f))
            {
                TimelineWidget->SetHorizontalPadding(Left, RightPadding);
                LastAppliedLeftPadding = Left;
                LastAppliedRightPadding = RightPadding;
                bHasAppliedPadding = true;
            }
        }
    }

    if (ActiveSequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Playing)
    {
        // CAK proved that OnGlobalTimeChanged can be too coarse during playback
        // in some settings. Pull the sub-frame local time once per Slate tick.
        HandleSequencerTimeChanged();
    }
}

void FWeaverSequencerBridge::HandleSequencerTimeChanged()
{
    if (bNotifyingTime) { return; }
    TGuardValue<bool> Guard(bNotifyingTime, true);
    const TSharedPtr<ISequencer> ActiveSequencer = Sequencer.Pin();
    if (!ActiveSequencer.IsValid())
    {
        return;
    }

    const FFrameRate TimelineRate = ResolveDisplayRate(ActiveSequencer);
    const FQualifiedFrameTime LocalTime = ActiveSequencer->GetLocalTime();
    const double NewCurrentFrame = FFrameRate::TransformTime(
        LocalTime.Time,
        LocalTime.Rate,
        TimelineRate).AsDecimal();

    OnSequencerFrameChanged.ExecuteIfBound(NewCurrentFrame);

    if (const TSharedPtr<SWeaverTimeline> TimelineWidget = Timeline.Pin())
    {
        TimelineWidget->Invalidate(EInvalidateWidgetReason::Paint);
    }
}

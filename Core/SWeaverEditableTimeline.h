#pragma once

#include "SWeaverTimeline.h"
#include "WeaverTimelineEditController.h"
#include "WeaverSequencerBridge.h"
#include "Widgets/SCompoundWidget.h"

/** Default editable entry point. Consumers supply authority/semantics, never Finished/SetBlocks wiring. */
class SWeaverEditableTimeline final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWeaverEditableTimeline) : _SyncSequencer(false), _JumpToEndpointOnClick(true), _EmbeddedInSequencer(false), _AllowTrackAreaScrub(true), _SeparateEndpointActions(false), _ActiveEndpointIsStart(true) {}
        SLATE_ARGUMENT(TSharedPtr<IWeaverTimelineEditAdapter>, Adapter)
        SLATE_ARGUMENT(bool, SyncSequencer)
        SLATE_ARGUMENT(bool, JumpToEndpointOnClick)
        /** Uses the owning editor explicitly. Embedded mode never falls back to global discovery. */
        SLATE_ARGUMENT(TWeakPtr<ISequencer>, SequencerSource)
        SLATE_ARGUMENT(bool, EmbeddedInSequencer)
        SLATE_ARGUMENT(bool, AllowTrackAreaScrub)
        SLATE_ARGUMENT(bool, SeparateEndpointActions)
        SLATE_ARGUMENT(FText, StartEndpointLabel)
        SLATE_ARGUMENT(FText, EndEndpointLabel)
        SLATE_ATTRIBUTE(FGuid, ActiveEndpointBlock)
        SLATE_ATTRIBUTE(bool, ActiveEndpointIsStart)
        SLATE_ARGUMENT(FWeaverSequencerBridge::FDisplayRateProvider, DisplayRateProvider)
        SLATE_EVENT(FOnWeaverFrameChanged, OnFrameChanged)
        SLATE_EVENT(FOnWeaverSelectionChanged, OnSelectionChanged)
        SLATE_EVENT(FOnWeaverContextRequested, OnContextRequested)
        SLATE_EVENT(FOnWeaverLaneContextRequested, OnLaneContextRequested)
        SLATE_EVENT(FOnWeaverBlockEndpointClicked, OnBlockEndpointClicked)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    virtual ~SWeaverEditableTimeline();
    virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
    TSharedRef<FWeaverTimelineEditController> GetController() const { return Controller.ToSharedRef(); }
    TSharedRef<SWeaverTimeline> GetTimeline() const { return Timeline.ToSharedRef(); }
    void Deactivate();
    /** Terminal owner teardown, including when another Slate parent still retains this widget. */
    void Shutdown();
    void SetCurrentFrame(double Frame);
    double GetCurrentFrame() const { return CurrentFrame; }
private:
    void Begin(EWeaverEditTarget Target, FGuid Lane, FGuid Item, EWeaverBlockEditKind Kind, FName Row = NAME_None);
    void FrameChanged(double Frame);
    TSharedPtr<SWeaverTimeline> Timeline;
    TSharedPtr<FWeaverTimelineEditController> Controller;
    FWeaverSequencerBridge Bridge;
    FOnWeaverFrameChanged OnFrameChanged;
    FOnWeaverBlockEndpointClicked OnBlockEndpointClicked;
    bool bJumpToEndpointOnClick = true;
    double CurrentFrame = 0;
    bool bNotifyingFrame = false;
    bool bShutdown = false;
    bool bRequiresOwner = false;
};

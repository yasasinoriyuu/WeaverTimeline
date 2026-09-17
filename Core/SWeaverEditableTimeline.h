#pragma once

#include "SWeaverTimeline.h"
#include "WeaverTimelineEditController.h"
#include "WeaverSequencerBridge.h"
#include "Widgets/SCompoundWidget.h"

/** Default editable entry point. Consumers supply authority/semantics, never Finished/SetBlocks wiring. */
class SWeaverEditableTimeline final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWeaverEditableTimeline) : _SyncSequencer(false), _JumpToEndpointOnClick(true) {}
        SLATE_ARGUMENT(TSharedPtr<IWeaverTimelineEditAdapter>, Adapter)
        SLATE_ARGUMENT(bool, SyncSequencer)
        SLATE_ARGUMENT(bool, JumpToEndpointOnClick)
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
};

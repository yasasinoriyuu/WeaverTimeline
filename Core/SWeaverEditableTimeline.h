#pragma once

#include "CoreMinimal.h"
#include "SWeaverTimeline.h"
#include "WeaverTimelineEditController.h"
#include "Widgets/SCompoundWidget.h"

class SWeaverEditableTimeline final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWeaverEditableTimeline)
        : _CurrentFrame(0.0)
        , _RulerHeight(24.0f)
        , _LaneHeight(36.0f)
        , _LabelWidth(148.0f)
    {}
        SLATE_ATTRIBUTE(double, CurrentFrame)
        SLATE_ARGUMENT(float, RulerHeight)
        SLATE_ARGUMENT(float, LaneHeight)
        SLATE_ARGUMENT(float, LabelWidth)
        SLATE_ARGUMENT(TSharedPtr<FWeaverTimelineEditController>, EditController)
        SLATE_EVENT(FOnWeaverFrameChanged, OnFrameChanged)
        SLATE_EVENT(FOnWeaverSelectionChanged, OnSelectionChanged)
        SLATE_EVENT(FOnWeaverDeleteRequested, OnDeleteRequested)
        SLATE_EVENT(FOnWeaverContextRequested, OnContextRequested)
        SLATE_EVENT(FOnWeaverViewRangeChanged, OnViewRangeChanged)
        SLATE_EVENT(FOnWeaverLaneHeaderActionRequested, OnLaneHeaderActionRequested)
        SLATE_EVENT(FOnWeaverBlockEndpointClicked, OnBlockEndpointClicked)
        SLATE_EVENT(FOnWeaverLaneContextRequested, OnLaneContextRequested)
        SLATE_EVENT(FOnWeaverBlockExpansionChanged, OnBlockExpansionChanged)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SWeaverEditableTimeline() override;

    TSharedPtr<SWeaverTimeline> GetTimeline() const { return Timeline; }
    TSharedPtr<FWeaverTimelineEditController> GetEditController() const { return EditController; }

    void RefreshFromSource();

    void SetSelection(const FWeaverSelection& InSelection);
    void ClearSelection();
    const FWeaverSelection* GetSelection() const;

    void SetExternalViewRange(double StartFrame, double EndFrame);
    void ClearExternalViewRange();
    void SetHorizontalPadding(float Left, float Right);

private:
    void HandleBlockExpansionChanged(FGuid BlockId, bool bExpanded);

    TSharedPtr<SWeaverTimeline> Timeline;
    TSharedPtr<FWeaverTimelineEditController> EditController;
    FOnWeaverBlockExpansionChanged ExternalOnBlockExpansionChanged;
};

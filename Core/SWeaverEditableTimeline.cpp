#include "SWeaverEditableTimeline.h"

void SWeaverEditableTimeline::Construct(const FArguments& InArgs)
{
    EditController = InArgs._EditController;
    ExternalOnBlockExpansionChanged = InArgs._OnBlockExpansionChanged;

    checkf(
        EditController.IsValid(),
        TEXT("SWeaverEditableTimeline requires a valid FWeaverTimelineEditController."));

    const TSharedRef<FWeaverTimelineEditController> ControllerRef = EditController.ToSharedRef();

    ChildSlot
    [
        SAssignNew(Timeline, SWeaverTimeline)
        .CurrentFrame(InArgs._CurrentFrame)
        .RulerHeight(InArgs._RulerHeight)
        .LaneHeight(InArgs._LaneHeight)
        .LabelWidth(InArgs._LabelWidth)
        .OnFrameChanged(InArgs._OnFrameChanged)
        .OnSelectionChanged(InArgs._OnSelectionChanged)
        .OnKeyEditStarted(FOnWeaverKeyEditStarted::CreateSP(
            ControllerRef,
            &FWeaverTimelineEditController::HandleKeyEditStarted))
        .OnKeyEditChanged(FOnWeaverKeyEditChanged::CreateSP(
            ControllerRef,
            &FWeaverTimelineEditController::HandleKeyEditChanged))
        .OnKeyEditFinished(FOnWeaverKeyEditFinished::CreateSP(
            ControllerRef,
            &FWeaverTimelineEditController::HandleKeyEditFinished))
        .OnBlockEditStarted(FOnWeaverBlockEditStarted::CreateSP(
            ControllerRef,
            &FWeaverTimelineEditController::HandleBlockEditStarted))
        .OnBlockEditChanged(FOnWeaverBlockEditChanged::CreateSP(
            ControllerRef,
            &FWeaverTimelineEditController::HandleBlockEditChanged))
        .OnBlockEditFinished(FOnWeaverBlockEditFinished::CreateSP(
            ControllerRef,
            &FWeaverTimelineEditController::HandleBlockEditFinished))
        .OnDeleteRequested(InArgs._OnDeleteRequested)
        .OnContextRequested(InArgs._OnContextRequested)
        .OnViewRangeChanged(InArgs._OnViewRangeChanged)
        .OnLaneHeaderActionRequested(InArgs._OnLaneHeaderActionRequested)
        .OnBlockEndpointClicked(InArgs._OnBlockEndpointClicked)
        .OnLaneContextRequested(InArgs._OnLaneContextRequested)
        .OnBlockExpansionChanged(this, &SWeaverEditableTimeline::HandleBlockExpansionChanged)
        .OnTimingRowEditStarted(FOnWeaverTimingRowEditStarted::CreateSP(
            ControllerRef,
            &FWeaverTimelineEditController::HandleTimingRowEditStarted))
        .OnTimingRowEditChanged(FOnWeaverTimingRowEditChanged::CreateSP(
            ControllerRef,
            &FWeaverTimelineEditController::HandleTimingRowEditChanged))
        .OnTimingRowEditFinished(FOnWeaverTimingRowEditFinished::CreateSP(
            ControllerRef,
            &FWeaverTimelineEditController::HandleTimingRowEditFinished))
    ];

    EditController->AttachTimeline(Timeline.ToSharedRef());
}

SWeaverEditableTimeline::~SWeaverEditableTimeline()
{
    if (EditController.IsValid())
    {
        EditController->DetachTimeline(Timeline);
    }
}

void SWeaverEditableTimeline::RefreshFromSource()
{
    if (EditController.IsValid())
    {
        EditController->RefreshFromSource();
    }
}

void SWeaverEditableTimeline::SetSelection(const FWeaverSelection& InSelection)
{
    if (Timeline.IsValid())
    {
        Timeline->SetSelection(InSelection);
    }
}

void SWeaverEditableTimeline::ClearSelection()
{
    if (Timeline.IsValid())
    {
        Timeline->ClearSelection();
    }
}

const FWeaverSelection* SWeaverEditableTimeline::GetSelection() const
{
    return Timeline.IsValid() ? &Timeline->GetSelection() : nullptr;
}

void SWeaverEditableTimeline::SetExternalViewRange(
    const double StartFrame,
    const double EndFrame)
{
    if (Timeline.IsValid())
    {
        Timeline->SetExternalViewRange(StartFrame, EndFrame);
    }
}

void SWeaverEditableTimeline::ClearExternalViewRange()
{
    if (Timeline.IsValid())
    {
        Timeline->ClearExternalViewRange();
    }
}

void SWeaverEditableTimeline::SetHorizontalPadding(
    const float Left,
    const float Right)
{
    if (Timeline.IsValid())
    {
        Timeline->SetHorizontalPadding(Left, Right);
    }
}

void SWeaverEditableTimeline::HandleBlockExpansionChanged(
    const FGuid BlockId,
    const bool bExpanded)
{
    if (EditController.IsValid())
    {
        EditController->SetBlockExpanded(BlockId, bExpanded);
    }
    ExternalOnBlockExpansionChanged.ExecuteIfBound(BlockId, bExpanded);
}

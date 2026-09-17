#include "SWeaverEditableTimeline.h"

void SWeaverEditableTimeline::Construct(const FArguments& InArgs)
{
    check(InArgs._Adapter.IsValid());
    Controller = MakeShared<FWeaverTimelineEditController>(InArgs._Adapter.ToSharedRef());
    OnFrameChanged = InArgs._OnFrameChanged;
    ChildSlot
    [
        SAssignNew(Timeline, SWeaverTimeline)
        .DeferDeleteSelectionToSource(true)
        .CurrentFrame_Lambda([this]() { return CurrentFrame; })
        .OnFrameChanged_Lambda([this](double Frame) { FrameChanged(Frame); Bridge.PushTimelineFrame(Frame); })
        .OnViewRangeChanged_Lambda([this](double Start, double End) { Bridge.PushTimelineViewRange(Start, End); })
        .OnSelectionChanged(InArgs._OnSelectionChanged)
        .OnContextRequested(InArgs._OnContextRequested)
        .OnLaneContextRequested(InArgs._OnLaneContextRequested)
        .OnKeyEditStarted_Lambda([this](FGuid Lane, FGuid Item, double)
            { Begin(EWeaverEditTarget::Key, Lane, Item, EWeaverBlockEditKind::Move); })
        .OnKeyEditChanged_Lambda([this](FGuid, FGuid, double Frame) { Controller->UpdateEdit(Frame, Frame); })
        .OnKeyEditFinished_Lambda([this](FGuid, FGuid, double Frame, bool Cancelled) { Controller->FinishEdit(Frame, Frame, Cancelled); })
        .OnBlockEditStarted_Lambda([this](FGuid Lane, FGuid Item, EWeaverBlockEditKind Kind)
            { Begin(EWeaverEditTarget::Block, Lane, Item, Kind); })
        .OnBlockEditChanged_Lambda([this](FGuid, FGuid, EWeaverBlockEditKind, double Start, double End)
            { Controller->UpdateEdit(Start, End); })
        .OnBlockEditFinished_Lambda([this](FGuid, FGuid, double Start, double End, bool Cancelled)
            { Controller->FinishEdit(Start, End, Cancelled); })
        .OnTimingRowEditStarted_Lambda([this](FGuid Lane, FGuid Item, FName Row, float, float)
            { Begin(EWeaverEditTarget::TimingRow, Lane, Item, EWeaverBlockEditKind::Move, Row); })
        .OnTimingRowEditChanged_Lambda([this](FGuid, FGuid, FName, float Start, float End) { Controller->UpdateEdit(Start, End); })
        .OnTimingRowEditFinished_Lambda([this](FGuid, FGuid, FName, float Start, float End, bool Cancelled)
            { Controller->FinishEdit(Start, End, Cancelled); })
        .OnBlockExpansionChanged_Lambda([this](FGuid Item, bool Expanded) { Controller->SetBlockExpanded(Item, Expanded); })
        .OnDeleteRequested_Lambda([this](EWeaverItemType Type, FGuid Lane, FGuid Item)
        {
            FWeaverEditProposal Command;
            Command.Target = EWeaverEditTarget::Command;
            Command.CommandId = TEXT("Delete");
            Command.ItemType = Type;
            Command.LaneId = Lane;
            Command.ItemId = Item;
            Controller->ExecuteCommand(Command);
        })
        .OnLaneHeaderActionRequested_Lambda([this](FGuid Lane, FName Action)
        {
            FWeaverEditProposal Command;
            Command.Target = EWeaverEditTarget::Command;
            Command.CommandId = Action;
            Command.LaneId = Lane;
            Controller->ExecuteCommand(Command);
        })
        .OnBlockEndpointClicked_Lambda([this](FGuid, FGuid Item, bool Start)
        {
            for (const auto& Block : Controller->GetPresentation().Blocks)
            {
                if (Block.BlockId == Item)
                {
                    const double Frame = Start ? Block.StartFrame : Block.EndFrame;
                    FrameChanged(Frame);
                    Bridge.PushTimelineFrame(Frame);
                    break;
                }
            }
        })
    ];
    Controller->AttachTimeline(Timeline.ToSharedRef());
    if (InArgs._SyncSequencer)
    {
        Bridge.Register(Timeline.ToSharedRef(), InArgs._DisplayRateProvider,
            FOnWeaverSequencerFrameChanged::CreateSP(this, &SWeaverEditableTimeline::FrameChanged),
            FSimpleDelegate::CreateLambda([Weak = TWeakPtr<FWeaverTimelineEditController>(Controller)]()
            {
                if (auto Pinned = Weak.Pin()) { Pinned->CancelEdit(EWeaverEditEndReason::SourceChanged); }
            }));
    }
}

SWeaverEditableTimeline::~SWeaverEditableTimeline()
{
    Bridge.Unregister();
    if (Controller) { Controller->DetachTimeline(); }
    if (Timeline) { Timeline->UnbindCallbacks(); }
}

void SWeaverEditableTimeline::Tick(const FGeometry& Geometry, double Time, float DeltaTime)
{
    Controller->Tick();
    Bridge.Sync(Timeline->GetCachedGeometry());
    SCompoundWidget::Tick(Geometry, Time, DeltaTime);
}

void SWeaverEditableTimeline::Begin(EWeaverEditTarget Target, FGuid Lane, FGuid Item, EWeaverBlockEditKind Kind, FName Row)
{
    FWeaverEditProposal Proposal;
    Proposal.Target = Target;
    Proposal.ItemType = Target == EWeaverEditTarget::Key ? EWeaverItemType::Key : EWeaverItemType::Block;
    Proposal.LaneId = Lane;
    Proposal.ItemId = Item;
    Proposal.BlockKind = Kind;
    Proposal.RowId = Row;
    if (!Controller->BeginEdit(Proposal)) { Timeline->CancelInteraction(); }
}

void SWeaverEditableTimeline::Deactivate()
{
    Controller->CancelEdit(EWeaverEditEndReason::Hidden);
}

void SWeaverEditableTimeline::SetCurrentFrame(double Frame)
{
    if (!FMath::IsFinite(Frame)) { return; }
    CurrentFrame = Frame;
    Timeline->Invalidate(EInvalidateWidgetReason::Paint);
}

void SWeaverEditableTimeline::FrameChanged(double Frame)
{
    if (bNotifyingFrame || !FMath::IsFinite(Frame)) { return; }
    TGuardValue<bool> Guard(bNotifyingFrame, true);
    SetCurrentFrame(Frame);
    OnFrameChanged.ExecuteIfBound(Frame);
}

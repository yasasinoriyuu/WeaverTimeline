#include "WeaverTimelineEditController.h"
#include "SWeaverTimeline.h"

FWeaverTimelineEditController::FWeaverTimelineEditController(TSharedRef<IWeaverTimelineEditAdapter> InAdapter)
    : Adapter(InAdapter)
{
    SourceChangedHandle = Adapter->OnSourceChanged.AddRaw(this, &FWeaverTimelineEditController::HandleSourceChanged);
}

FWeaverTimelineEditController::~FWeaverTimelineEditController()
{
    Adapter->OnSourceChanged.Remove(SourceChangedHandle);
    DetachTimeline();
}

void FWeaverTimelineEditController::AttachTimeline(TSharedRef<SWeaverTimeline> InTimeline)
{
    DetachTimeline();
    Timeline = InTimeline;
    bAttached = true;
    RefreshFromSource();
}

void FWeaverTimelineEditController::DetachTimeline()
{
    bAttached = false; // Reentrant callbacks cannot start another edit during detach.
    CancelEdit(EWeaverEditEndReason::Detached);
    Timeline.Reset();
}

void FWeaverTimelineEditController::HandleSourceChanged()
{
    bSourceInvalidated = true;
    RefreshFromSource();
}

void FWeaverTimelineEditController::Tick()
{
    if (!bDispatching && PendingCancelReason.IsSet())
    {
        const auto Reason = *PendingCancelReason;
        PendingCancelReason.Reset();
        CancelEdit(Reason);
    }
    if (bRefreshPending || bSourceInvalidated || Adapter->GetContext() != PresentedContext)
    {
        RefreshFromSource();
    }
}

void FWeaverTimelineEditController::RefreshFromSource()
{
    if (bDispatching)
    {
        bRefreshPending = true;
        return;
    }
    if (ActiveSession.IsSet() && (bSourceInvalidated || Adapter->GetContext() != ActiveSession->Context))
    {
        CancelEdit(EWeaverEditEndReason::SourceChanged);
        return; // Complete performs the read, even if cancellation had no data mutation.
    }
    if (bSourceInvalidated || Adapter->GetContext() != PresentedContext)
    {
        // Also cancel pending endpoint clicks, scrub and pan before publishing a new context.
        if (auto Widget = Timeline.Pin()) { Widget->CancelInteraction(); }
    }
    TGuardValue<bool> Guard(bDispatching, true);
    bRefreshPending = false;
    bSourceInvalidated = false;
    const FWeaverSourceContext Context = Adapter->GetContext();
    FWeaverTimelinePresentation Fresh;
    Adapter->BuildPresentation(Fresh);
    if (Context != Adapter->GetContext())
    {
        bRefreshPending = true; // Never publish a torn source/context snapshot.
        return;
    }
    if (PresentedContext.Id != Context.Id)
    {
        ExpansionOverrides.Reset();
        if (auto Widget = Timeline.Pin()) { Widget->ClearSelection(true); }
    }
    PresentedContext = Context;
    Presentation = MoveTemp(Fresh);
    ApplyPresentation();
    OnReconciled.Broadcast(PresentedContext, LastResult);
}

void FWeaverTimelineEditController::ApplyPresentation()
{
    TSet<FGuid> ExistingBlocks;
    for (FWeaverBlock& Block : Presentation.Blocks)
    {
        ExistingBlocks.Add(Block.BlockId);
        if (const bool* Expanded = ExpansionOverrides.Find(Block.BlockId)) { Block.bExpanded = *Expanded; }
    }
    for (auto It = ExpansionOverrides.CreateIterator(); It; ++It)
    {
        if (!ExistingBlocks.Contains(It.Key())) { It.RemoveCurrent(); }
    }
    if (auto Widget = Timeline.Pin())
    {
        Widget->SetPresentation(Presentation.Lanes, Presentation.Keys, Presentation.Blocks);
    }
}

bool FWeaverTimelineEditController::ReadOriginal(FWeaverEditProposal& Proposal) const
{
    if (Proposal.Target == EWeaverEditTarget::Command) { return true; }
    const auto* Lane = Presentation.Lanes.FindByPredicate([&](const FWeaverLane& Candidate) { return Candidate.LaneId == Proposal.LaneId; });
    if (!Lane || !Lane->bEnabled) { return false; }
    if (Proposal.Target == EWeaverEditTarget::Key)
    {
        for (const FWeaverKey& Key : Presentation.Keys)
        {
            if (Key.KeyId == Proposal.ItemId && Key.LaneId == Proposal.LaneId && Key.bEnabled)
            {
                Proposal.Start = Proposal.End = Key.Frame;
                return true;
            }
        }
        return false;
    }
    for (const FWeaverBlock& Block : Presentation.Blocks)
    {
        if (Block.BlockId != Proposal.ItemId || Block.LaneId != Proposal.LaneId || !Block.bEnabled) { continue; }
        if (Proposal.Target == EWeaverEditTarget::Block)
        {
            if (Proposal.BlockKind != EWeaverBlockEditKind::Move && !Block.bResizable) { return false; }
            Proposal.Start = Block.StartFrame;
            Proposal.End = Block.EndFrame;
            return true;
        }
        for (const FWeaverTimingRow& Row : Block.TimingRows)
        {
            if (Row.RowId == Proposal.RowId && Row.bEnabled)
            {
                Proposal.Start = Row.StartRatio;
                Proposal.End = Row.EndRatio;
                return true;
            }
        }
    }
    return false;
}

bool FWeaverTimelineEditController::BeginEdit(const FWeaverEditProposal& Proposal)
{
    if (bDispatching || !bAttached || ActiveSession.IsSet()) { return false; }
    if (bSourceInvalidated || Adapter->GetContext() != PresentedContext)
    {
        RefreshFromSource();
        return false; // The pointer hit belongs to the old presentation. Require a fresh gesture.
    }
    RefreshFromSource();
    FWeaverEditProposal Original = Proposal;
    if (!PresentedContext.Id.IsValid() || Adapter->GetContext() != PresentedContext || !ReadOriginal(Original))
    {
        if (auto Widget = Timeline.Pin()) { Widget->CancelInteraction(); }
        return false;
    }
    FWeaverEditSession Session;
    Session.Id = FGuid::NewGuid();
    Session.Context = PresentedContext;
    Session.Original = Original;
    Session.Proposal = Original;
    ActiveSession = Session;
    {
        TGuardValue<bool> Guard(bDispatching, true);
        Adapter->BeginPreview(Session);
    }
    Tick(); // Source replacement during BeginPreview cancels before the next pointer event.
    return ActiveSession.IsSet();
}

void FWeaverTimelineEditController::UpdateEdit(double Start, double End)
{
    if (bDispatching || !ActiveSession.IsSet()) { return; }
    if (bSourceInvalidated || Adapter->GetContext() != ActiveSession->Context)
    {
        CancelEdit(EWeaverEditEndReason::SourceChanged);
        return;
    }
    if (!FMath::IsFinite(Start) || !FMath::IsFinite(End)) { CancelEdit(); return; }
    ActiveSession->Proposal.Start = Start;
    ActiveSession->Proposal.End = End;
    const FWeaverEditSession Snapshot = *ActiveSession;
    {
        TGuardValue<bool> Guard(bDispatching, true);
        Adapter->UpdatePreview(Snapshot);
    }
    Tick();
}

void FWeaverTimelineEditController::FinishEdit(double Start, double End, bool bCancelled)
{
    if (bDispatching || !ActiveSession.IsSet()) { return; }
    if (bCancelled || !FMath::IsFinite(Start) || !FMath::IsFinite(End)) { CancelEdit(); return; }
    ActiveSession->Proposal.Start = Start;
    ActiveSession->Proposal.End = End;
    Complete(true, EWeaverEditEndReason::Finished);
}

void FWeaverTimelineEditController::CancelEdit(EWeaverEditEndReason Reason)
{
    if (bDispatching)
    {
        // Defer until the current external call returns. Never end a preview in its own callback.
        PendingCancelReason = Reason;
        bRefreshPending = true;
        return;
    }
    if (ActiveSession.IsSet()) { Complete(false, Reason); }
    else if (auto Widget = Timeline.Pin()) { Widget->CancelInteraction(); }
}

void FWeaverTimelineEditController::Complete(bool bCommit, EWeaverEditEndReason Reason)
{
    if (bDispatching || !ActiveSession.IsSet()) { return; }
    const FWeaverEditSession Session = *ActiveSession;
    ActiveSession.Reset(); // Exactly-once termination, before any consumer or Slate callback.
    {
        TGuardValue<bool> Guard(bDispatching, true);
        if (auto Widget = Timeline.Pin()) { Widget->CancelInteraction(); }
        LastResult = { EWeaverEditOutcome::Cancelled, FText::GetEmpty() };
        const auto CanCommit = [&]()
        {
            if (PendingCancelReason.IsSet()) { Reason = *PendingCancelReason; return false; }
            if (!bAttached) { Reason = EWeaverEditEndReason::Detached; return false; }
            if (bSourceInvalidated || Adapter->GetContext() != Session.Context)
            {
                Reason = EWeaverEditEndReason::SourceChanged;
                return false;
            }
            return true;
        };
        if (bCommit && CanCommit())
        {
            auto Transaction = Adapter->BeginTransaction(Session);
            // Transaction setup is consumer code: cancellation here must prevent the first write.
            if (CanCommit())
            {
                LastResult = Adapter->Commit(Session);
            }
            if (Transaction) { Transaction->Finish(LastResult.Outcome); }
        }
        Adapter->EndPreview(Session, LastResult, Reason);
    }
    PendingCancelReason.Reset(); // Session is already terminal; no second EndPreview.
    RefreshFromSource(); // Full authoritative pull for Applied, Rejected, NoChange and Cancelled.
}

FWeaverEditResult FWeaverTimelineEditController::ExecuteCommand(const FWeaverEditProposal& Command)
{
    if (bDispatching || !bAttached || Command.Target != EWeaverEditTarget::Command)
    {
        return { EWeaverEditOutcome::Rejected, FText::FromString(TEXT("当前无法执行命令")) };
    }
    CancelEdit();
    RefreshFromSource();
    if (BeginEdit(Command)) { Complete(true, EWeaverEditEndReason::Finished); }
    else { LastResult = { EWeaverEditOutcome::Rejected, FText::FromString(TEXT("数据源不可编辑")) }; }
    return LastResult;
}

void FWeaverTimelineEditController::SetBlockExpanded(FGuid BlockId, bool bExpanded)
{
    ExpansionOverrides.Add(BlockId, bExpanded);
    RefreshFromSource();
}

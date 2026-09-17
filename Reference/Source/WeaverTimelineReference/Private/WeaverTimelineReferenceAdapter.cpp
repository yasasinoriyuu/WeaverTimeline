#include "WeaverTimelineReferenceAdapter.h"
#include "ScopedTransaction.h"

namespace
{
class FWeaverReferenceTransaction final : public IWeaverEditTransaction
{
public:
    FWeaverReferenceTransaction() : Transaction(FText::FromString(TEXT("编辑参考编排"))) {}
    virtual void Finish(EWeaverEditOutcome Outcome) override
    {
        if (Outcome != EWeaverEditOutcome::Applied) { Transaction.Cancel(); }
    }
private:
    FScopedTransaction Transaction;
};
}

FWeaverTimelineReferenceAdapter::FWeaverTimelineReferenceAdapter() { ReplaceDocument(); }

FWeaverTimelineReferenceAdapter::~FWeaverTimelineReferenceAdapter()
{
    if (Document.IsValid()) { Document->OnChanged.Remove(DocumentChangedHandle); }
}

void FWeaverTimelineReferenceAdapter::ReplaceDocument()
{
    if (Document.IsValid()) { Document->OnChanged.Remove(DocumentChangedHandle); }
    Document.Reset(NewObject<UWeaverReferenceDocument>(GetTransientPackage(), NAME_None, RF_Transactional));
    Document->LaneId = FGuid::NewGuid();
    FWeaverReferenceItem A;
    A.Id = FGuid::NewGuid();
    FWeaverReferenceItem B;
    B.Id = FGuid::NewGuid(); B.Start = 70; B.End = 100;
    FWeaverReferenceItem Key;
    Key.Id = FGuid::NewGuid(); Key.bKey = true; Key.Start = Key.End = 130;
    Document->Items = { A, B, Key };
    DocumentChangedHandle = Document->OnChanged.AddRaw(this, &FWeaverTimelineReferenceAdapter::NotifySourceChanged);
    NotifySourceChanged();
}

void FWeaverTimelineReferenceAdapter::NotifySourceChanged()
{
    ++Revision; // Deliberately outside Undo serialization.
    OnSourceChanged.Broadcast();
}

FWeaverSourceContext FWeaverTimelineReferenceAdapter::GetContext() const
{
    return { Document->ContextId, Revision };
}

void FWeaverTimelineReferenceAdapter::BuildPresentation(FWeaverTimelinePresentation& Out) const
{
    Out = {};
    FWeaverLane Lane;
    Lane.LaneId = Document->LaneId;
    Lane.Label = FText::FromString(TEXT("参考数据"));
    FWeaverLaneHeaderAction Action;
    Action.ActionId = TEXT("Create");
    Action.Label = FText::FromString(TEXT("新增"));
    Lane.HeaderActions.Add(Action);
    Out.Lanes.Add(Lane);
    for (const auto& Item : Document->Items)
    {
        if (Item.bKey)
        {
            FWeaverKey Key;
            Key.KeyId = Item.Id; Key.LaneId = Lane.LaneId; Key.Frame = Item.Start;
            Out.Keys.Add(Key);
        }
        else
        {
            FWeaverBlock Block;
            Block.BlockId = Item.Id; Block.LaneId = Lane.LaneId;
            Block.StartFrame = Item.Start; Block.EndFrame = Item.End;
            Block.Label = FText::FromString(TEXT("参考片段"));
            FWeaverTimingRow Row;
            Row.RowId = TEXT("Weight"); Row.Label = FText::FromString(TEXT("生效区间"));
            Row.StartRatio = Item.TimingStart; Row.EndRatio = Item.TimingEnd;
            Block.TimingRows.Add(Row);
            Out.Blocks.Add(Block);
        }
    }
}

TUniquePtr<IWeaverEditTransaction> FWeaverTimelineReferenceAdapter::BeginTransaction(const FWeaverEditSession& Session)
{
    return MakeUnique<FWeaverReferenceTransaction>();
}

FWeaverEditResult FWeaverTimelineReferenceAdapter::Commit(const FWeaverEditSession& Session)
{
    ++CommitCount;
    if (DuringCommit) { DuringCommit(); }
    if (Session.Context != GetContext()) { return { EWeaverEditOutcome::Rejected, FText::FromString(TEXT("数据源已变化")) }; }
    if (Policy == EWeaverReferencePolicy::Reject) { return { EWeaverEditOutcome::Rejected, FText::FromString(TEXT("参考策略拒绝了本次编辑")) }; }
    const auto& Proposal = Session.Proposal;
    TArray<FWeaverReferenceItem> Updated = Document->Items;
    if (Proposal.Target == EWeaverEditTarget::Command)
    {
        if (Proposal.CommandId == TEXT("Delete"))
        {
            Updated.RemoveAll([&](const auto& Item) { return Item.Id == Proposal.ItemId; });
        }
        else if (Proposal.CommandId == TEXT("Create"))
        {
            FWeaverReferenceItem Item;
            Item.Id = FGuid::NewGuid(); Item.Start = Proposal.Start; Item.End = Proposal.Start + 30;
            Updated.Add(Item);
        }
        else { return {}; }
    }
    else
    {
        auto* Item = Updated.FindByPredicate([&](const auto& Candidate) { return Candidate.Id == Proposal.ItemId; });
        if (!Item) { return {}; }
        if (Proposal.Target == EWeaverEditTarget::TimingRow)
        {
            if (Proposal.RowId != TEXT("Weight")) { return {}; }
            Item->TimingStart = FMath::Clamp(Proposal.Start, 0.0, 1.0);
            Item->TimingEnd = FMath::Clamp(Proposal.End, double(Item->TimingStart), 1.0);
        }
        else
        {
            double Start = Proposal.Start;
            double End = Proposal.End;
            if (Policy == EWeaverReferencePolicy::Snap)
            {
                Start = FMath::RoundToDouble(Start / 10.0) * 10.0;
                End = Proposal.BlockKind == EWeaverBlockEditKind::Move ? Start + Proposal.End - Proposal.Start
                    : FMath::RoundToDouble(End / 10.0) * 10.0;
            }
            if (Policy == EWeaverReferencePolicy::Clamp)
            {
                Start = FMath::Clamp(Start, 0.0, 200.0);
                End = FMath::Clamp(End, Start + 1.0, 240.0);
            }
            Item->Start = Start;
            Item->End = Item->bKey ? Start : FMath::Max(Start + 1.0, End);
            if (Policy == EWeaverReferencePolicy::Cascade && !Item->bKey)
            {
                double NextStart = Item->End;
                for (auto& Following : Updated)
                {
                    if (Following.bKey || Following.Id == Item->Id || Following.Start < Session.Original.End) { continue; }
                    const double Duration = Following.End - Following.Start;
                    Following.Start = FMath::Max(Following.Start, NextStart);
                    Following.End = Following.Start + Duration;
                    NextStart = Following.End;
                }
            }
        }
    }
    if (Updated == Document->Items) { return { EWeaverEditOutcome::NoChange, FText::GetEmpty() }; }
    Document->Modify();
    Document->Items = MoveTemp(Updated);
    NotifySourceChanged();
    return { EWeaverEditOutcome::Applied, FText::GetEmpty() };
}

void FWeaverTimelineReferenceAdapter::BeginPreview(const FWeaverEditSession& Session)
{
    ++BeginPreviewCount;
    Previews.Add(Session.Id, Session);
}

void FWeaverTimelineReferenceAdapter::UpdatePreview(const FWeaverEditSession& Session)
{
    Previews.Add(Session.Id, Session);
}

void FWeaverTimelineReferenceAdapter::EndPreview(const FWeaverEditSession& Session, const FWeaverEditResult&, EWeaverEditEndReason)
{
    ++EndPreviewCount;
    Previews.Remove(Session.Id); // Works even after Document was replaced.
}

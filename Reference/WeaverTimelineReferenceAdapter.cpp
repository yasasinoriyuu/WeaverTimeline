#include "WeaverTimelineReferenceAdapter.h"

namespace
{
FWeaverLane MakeLane(const FGuid& LaneId, const TCHAR* Label)
{
    FWeaverLane Lane;
    Lane.LaneId = LaneId;
    Lane.Label = FText::FromString(Label);
    return Lane;
}
}

FWeaverTimelineReferenceAdapter::FWeaverTimelineReferenceAdapter()
{
    ResetDemoData();
}

void FWeaverTimelineReferenceAdapter::ResetDemoData()
{
    State = FWeaverTimelinePresentation();
    bRejectNextCommit = false;

    const FGuid LaneA = FGuid::NewGuid();
    const FGuid LaneB = FGuid::NewGuid();

    FWeaverLane FirstLane = MakeLane(LaneA, TEXT("Reference Lane A"));
    FWeaverLaneHeaderAction EnabledAction;
    EnabledAction.ActionId = TEXT("EnabledAction");
    EnabledAction.Label = FText::FromString(TEXT("E"));
    FirstLane.HeaderActions.Add(EnabledAction);

    FWeaverLaneHeaderAction DisabledAction;
    DisabledAction.ActionId = TEXT("DisabledAction");
    DisabledAction.Label = FText::FromString(TEXT("D"));
    DisabledAction.bEnabled = false;
    FirstLane.HeaderActions.Add(DisabledAction);

    State.Lanes.Add(FirstLane);
    State.Lanes.Add(MakeLane(LaneB, TEXT("Reference Lane B")));

    FWeaverBlock Block;
    Block.BlockId = FGuid::NewGuid();
    Block.LaneId = LaneA;
    Block.StartFrame = 20.0;
    Block.EndFrame = 140.0;
    Block.Label = FText::FromString(TEXT("Reference Block"));

    FWeaverTimingRow PositionRow;
    PositionRow.RowId = TEXT("Position");
    PositionRow.Label = FText::FromString(TEXT("Position"));
    PositionRow.StartRatio = 0.0f;
    PositionRow.EndRatio = 1.0f;
    Block.TimingRows.Add(PositionRow);

    FWeaverTimingRow YawRow;
    YawRow.RowId = TEXT("Yaw");
    YawRow.Label = FText::FromString(TEXT("Yaw"));
    YawRow.StartRatio = 0.25f;
    YawRow.EndRatio = 0.75f;
    Block.TimingRows.Add(YawRow);

    FWeaverTimingRow PitchRow;
    PitchRow.RowId = TEXT("Pitch");
    PitchRow.Label = FText::FromString(TEXT("Pitch"));
    PitchRow.StartRatio = 0.5f;
    PitchRow.EndRatio = 1.0f;
    Block.TimingRows.Add(PitchRow);

    State.Blocks.Add(Block);

    FWeaverKey Key;
    Key.KeyId = FGuid::NewGuid();
    Key.LaneId = LaneA;
    Key.Frame = 70.0;
    State.Keys.Add(Key);
}

void FWeaverTimelineReferenceAdapter::BuildPresentation(
    FWeaverTimelinePresentation& OutPresentation)
{
    OutPresentation = State;
}

bool FWeaverTimelineReferenceAdapter::ConsumeRejectNextCommit()
{
    if (!bRejectNextCommit)
    {
        return false;
    }
    bRejectNextCommit = false;
    return true;
}

EWeaverEditCommitResult FWeaverTimelineReferenceAdapter::CommitKeyEdit(
    const FGuid& LaneId,
    const FGuid& KeyId,
    const double FinalFrame)
{
    if (ConsumeRejectNextCommit())
    {
        return EWeaverEditCommitResult::Rejected;
    }

    FWeaverKey* Key = State.Keys.FindByPredicate(
        [&LaneId, &KeyId](const FWeaverKey& Candidate)
        {
            return Candidate.LaneId == LaneId && Candidate.KeyId == KeyId;
        });
    if (!Key)
    {
        return EWeaverEditCommitResult::Rejected;
    }

    Key->Frame = FinalFrame;
    return EWeaverEditCommitResult::Accepted;
}

EWeaverEditCommitResult FWeaverTimelineReferenceAdapter::CommitBlockEdit(
    const FGuid& LaneId,
    const FGuid& BlockId,
    const EWeaverBlockEditKind Kind,
    const double FinalStart,
    const double FinalEnd)
{
    if (ConsumeRejectNextCommit())
    {
        return EWeaverEditCommitResult::Rejected;
    }

    FWeaverBlock* Block = State.Blocks.FindByPredicate(
        [&LaneId, &BlockId](const FWeaverBlock& Candidate)
        {
            return Candidate.LaneId == LaneId && Candidate.BlockId == BlockId;
        });
    if (!Block)
    {
        return EWeaverEditCommitResult::Rejected;
    }

    Block->StartFrame = FinalStart;
    Block->EndFrame = FinalEnd;
    return EWeaverEditCommitResult::Accepted;
}

EWeaverEditCommitResult FWeaverTimelineReferenceAdapter::CommitTimingRowEdit(
    const FGuid& LaneId,
    const FGuid& BlockId,
    const FName RowId,
    const float FinalStartRatio,
    const float FinalEndRatio)
{
    if (ConsumeRejectNextCommit())
    {
        return EWeaverEditCommitResult::Rejected;
    }

    FWeaverBlock* Block = State.Blocks.FindByPredicate(
        [&LaneId, &BlockId](const FWeaverBlock& Candidate)
        {
            return Candidate.LaneId == LaneId && Candidate.BlockId == BlockId;
        });
    if (!Block)
    {
        return EWeaverEditCommitResult::Rejected;
    }

    FWeaverTimingRow* Row = Block->TimingRows.FindByPredicate(
        [RowId](const FWeaverTimingRow& Candidate)
        {
            return Candidate.RowId == RowId;
        });
    if (!Row)
    {
        return EWeaverEditCommitResult::Rejected;
    }

    Row->StartRatio = FinalStartRatio;
    Row->EndRatio = FinalEndRatio;
    return EWeaverEditCommitResult::Accepted;
}

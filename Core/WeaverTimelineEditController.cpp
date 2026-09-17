#include "WeaverTimelineEditController.h"

#include "SWeaverTimeline.h"

namespace
{
const FWeaverKey* FindKeyById(
    const FWeaverTimelinePresentation& Presentation,
    const FGuid& KeyId)
{
    return Presentation.Keys.FindByPredicate(
        [&KeyId](const FWeaverKey& Key)
        {
            return Key.KeyId == KeyId;
        });
}

const FWeaverBlock* FindBlockById(
    const FWeaverTimelinePresentation& Presentation,
    const FGuid& BlockId)
{
    return Presentation.Blocks.FindByPredicate(
        [&BlockId](const FWeaverBlock& Block)
        {
            return Block.BlockId == BlockId;
        });
}

const FWeaverTimingRow* FindTimingRowById(
    const FWeaverTimelinePresentation& Presentation,
    const FGuid& BlockId,
    const FName RowId)
{
    const FWeaverBlock* Block = FindBlockById(Presentation, BlockId);
    return Block
        ? Block->TimingRows.FindByPredicate(
            [RowId](const FWeaverTimingRow& Row)
            {
                return Row.RowId == RowId;
            })
        : nullptr;
}
}

FWeaverTimelineEditController::FWeaverTimelineEditController(
    TSharedRef<IWeaverTimelineEditAdapter> InAdapter)
    : Adapter(MoveTemp(InAdapter))
{
}

void FWeaverTimelineEditController::AttachTimeline(
    const TSharedRef<SWeaverTimeline>& InTimeline)
{
    Timeline = InTimeline;
    RefreshFromSource();
}

void FWeaverTimelineEditController::DetachTimeline(
    const TSharedPtr<SWeaverTimeline>& InTimeline)
{
    if (!InTimeline.IsValid() || Timeline.Pin() == InTimeline)
    {
        Timeline.Reset();
    }
}

FWeaverTimelinePresentation FWeaverTimelineEditController::BuildPresentation()
{
    FWeaverTimelinePresentation Presentation;
    Adapter->BuildPresentation(Presentation);

    TSet<FGuid> NextExpandedBlockIds;
    for (FWeaverBlock& Block : Presentation.Blocks)
    {
        if (ExpandedBlockIds.Contains(Block.BlockId) || Block.bExpanded)
        {
            Block.bExpanded = true;
            NextExpandedBlockIds.Add(Block.BlockId);
        }
    }
    ExpandedBlockIds = MoveTemp(NextExpandedBlockIds);
    return Presentation;
}

void FWeaverTimelineEditController::ApplyPresentation(
    FWeaverTimelinePresentation&& Presentation)
{
    if (const TSharedPtr<SWeaverTimeline> PinnedTimeline = Timeline.Pin())
    {
        PinnedTimeline->SetLanes(MoveTemp(Presentation.Lanes));
        PinnedTimeline->SetKeys(MoveTemp(Presentation.Keys));
        PinnedTimeline->SetBlocks(MoveTemp(Presentation.Blocks));
    }
}

void FWeaverTimelineEditController::RefreshFromSource()
{
    ApplyPresentation(BuildPresentation());
}

void FWeaverTimelineEditController::SetBlockExpanded(
    const FGuid& BlockId,
    const bool bExpanded)
{
    if (!BlockId.IsValid())
    {
        return;
    }

    if (bExpanded)
    {
        ExpandedBlockIds.Add(BlockId);
    }
    else
    {
        ExpandedBlockIds.Remove(BlockId);
    }
}

void FWeaverTimelineEditController::SetValidationError(const FString& Error)
{
    LastValidationError = Error;
    if (!LastValidationError.IsEmpty())
    {
        ensureMsgf(false, TEXT("WeaverTimeline edit invariant failed: %s"), *LastValidationError);
    }
}

void FWeaverTimelineEditController::ValidateCommittedKey(
    const FWeaverTimelinePresentation& Presentation,
    const FGuid& KeyId,
    const double FinalFrame,
    const EWeaverEditCommitResult CommitResult)
{
    if (CommitResult != EWeaverEditCommitResult::Accepted)
    {
        return;
    }

    const FWeaverKey* Key = FindKeyById(Presentation, KeyId);
    if (!Key)
    {
        SetValidationError(FString::Printf(
            TEXT("Accepted key commit removed KeyId=%s from authoritative presentation."),
            *KeyId.ToString()));
        return;
    }

    if (!FMath::IsNearlyEqual(Key->Frame, FinalFrame, 1e-6))
    {
        SetValidationError(FString::Printf(
            TEXT("Accepted key commit did not persist. KeyId=%s Requested=%.6f Authoritative=%.6f"),
            *KeyId.ToString(),
            FinalFrame,
            Key->Frame));
    }
}

void FWeaverTimelineEditController::ValidateCommittedBlock(
    const FWeaverTimelinePresentation& Presentation,
    const FGuid& BlockId,
    const double FinalStart,
    const double FinalEnd,
    const EWeaverEditCommitResult CommitResult)
{
    if (CommitResult != EWeaverEditCommitResult::Accepted)
    {
        return;
    }

    const FWeaverBlock* Block = FindBlockById(Presentation, BlockId);
    if (!Block)
    {
        SetValidationError(FString::Printf(
            TEXT("Accepted block commit removed BlockId=%s from authoritative presentation."),
            *BlockId.ToString()));
        return;
    }

    if (!FMath::IsNearlyEqual(Block->StartFrame, FinalStart, 1e-6)
        || !FMath::IsNearlyEqual(Block->EndFrame, FinalEnd, 1e-6))
    {
        SetValidationError(FString::Printf(
            TEXT("Accepted block commit did not persist. BlockId=%s Requested=[%.6f, %.6f] Authoritative=[%.6f, %.6f]"),
            *BlockId.ToString(),
            FinalStart,
            FinalEnd,
            Block->StartFrame,
            Block->EndFrame));
    }
}

void FWeaverTimelineEditController::ValidateCommittedTimingRow(
    const FWeaverTimelinePresentation& Presentation,
    const FGuid& BlockId,
    const FName RowId,
    const float FinalStartRatio,
    const float FinalEndRatio,
    const EWeaverEditCommitResult CommitResult)
{
    if (CommitResult != EWeaverEditCommitResult::Accepted)
    {
        return;
    }

    const FWeaverTimingRow* Row = FindTimingRowById(Presentation, BlockId, RowId);
    if (!Row)
    {
        SetValidationError(FString::Printf(
            TEXT("Accepted timing-row commit removed BlockId=%s RowId=%s from authoritative presentation."),
            *BlockId.ToString(),
            *RowId.ToString()));
        return;
    }

    if (!FMath::IsNearlyEqual(Row->StartRatio, FinalStartRatio, 1e-5f)
        || !FMath::IsNearlyEqual(Row->EndRatio, FinalEndRatio, 1e-5f))
    {
        SetValidationError(FString::Printf(
            TEXT("Accepted timing-row commit did not persist. BlockId=%s RowId=%s Requested=[%.5f, %.5f] Authoritative=[%.5f, %.5f]"),
            *BlockId.ToString(),
            *RowId.ToString(),
            FinalStartRatio,
            FinalEndRatio,
            Row->StartRatio,
            Row->EndRatio));
    }
}

void FWeaverTimelineEditController::HandleKeyEditStarted(
    const FGuid LaneId,
    const FGuid KeyId,
    const double OriginalFrame)
{
    LastValidationError.Reset();
    Adapter->BeginKeyEdit(LaneId, KeyId, OriginalFrame);
}

void FWeaverTimelineEditController::HandleKeyEditChanged(
    const FGuid LaneId,
    const FGuid KeyId,
    const double PreviewFrame)
{
    Adapter->PreviewKeyEdit(LaneId, KeyId, PreviewFrame);
}

void FWeaverTimelineEditController::HandleKeyEditFinished(
    const FGuid LaneId,
    const FGuid KeyId,
    const double FinalFrame,
    const bool bCancelled)
{
    EWeaverEditCommitResult CommitResult = EWeaverEditCommitResult::Rejected;
    if (bCancelled)
    {
        Adapter->CancelKeyEdit(LaneId, KeyId);
    }
    else
    {
        CommitResult = Adapter->CommitKeyEdit(LaneId, KeyId, FinalFrame);
    }

    FWeaverTimelinePresentation Presentation = BuildPresentation();
    ValidateCommittedKey(Presentation, KeyId, FinalFrame, CommitResult);
    ApplyPresentation(MoveTemp(Presentation));
}

void FWeaverTimelineEditController::HandleBlockEditStarted(
    const FGuid LaneId,
    const FGuid BlockId,
    const EWeaverBlockEditKind Kind)
{
    LastValidationError.Reset();
    ActiveBlockEditKind = Kind;
    Adapter->BeginBlockEdit(LaneId, BlockId, Kind);
}

void FWeaverTimelineEditController::HandleBlockEditChanged(
    const FGuid LaneId,
    const FGuid BlockId,
    const EWeaverBlockEditKind Kind,
    const double PreviewStart,
    const double PreviewEnd)
{
    ActiveBlockEditKind = Kind;
    Adapter->PreviewBlockEdit(
        LaneId,
        BlockId,
        Kind,
        PreviewStart,
        PreviewEnd);
}

void FWeaverTimelineEditController::HandleBlockEditFinished(
    const FGuid LaneId,
    const FGuid BlockId,
    const double FinalStart,
    const double FinalEnd,
    const bool bCancelled)
{
    EWeaverEditCommitResult CommitResult = EWeaverEditCommitResult::Rejected;
    if (bCancelled)
    {
        Adapter->CancelBlockEdit(LaneId, BlockId, ActiveBlockEditKind);
    }
    else
    {
        CommitResult = Adapter->CommitBlockEdit(
            LaneId,
            BlockId,
            ActiveBlockEditKind,
            FinalStart,
            FinalEnd);
    }

    FWeaverTimelinePresentation Presentation = BuildPresentation();
    ValidateCommittedBlock(
        Presentation,
        BlockId,
        FinalStart,
        FinalEnd,
        CommitResult);
    ApplyPresentation(MoveTemp(Presentation));
    ActiveBlockEditKind = EWeaverBlockEditKind::Move;
}

void FWeaverTimelineEditController::HandleTimingRowEditStarted(
    const FGuid LaneId,
    const FGuid BlockId,
    const FName RowId,
    const float OriginalStartRatio,
    const float OriginalEndRatio)
{
    LastValidationError.Reset();
    Adapter->BeginTimingRowEdit(
        LaneId,
        BlockId,
        RowId,
        OriginalStartRatio,
        OriginalEndRatio);
}

void FWeaverTimelineEditController::HandleTimingRowEditChanged(
    const FGuid LaneId,
    const FGuid BlockId,
    const FName RowId,
    const float PreviewStartRatio,
    const float PreviewEndRatio)
{
    Adapter->PreviewTimingRowEdit(
        LaneId,
        BlockId,
        RowId,
        PreviewStartRatio,
        PreviewEndRatio);
}

void FWeaverTimelineEditController::HandleTimingRowEditFinished(
    const FGuid LaneId,
    const FGuid BlockId,
    const FName RowId,
    const float FinalStartRatio,
    const float FinalEndRatio,
    const bool bCancelled)
{
    EWeaverEditCommitResult CommitResult = EWeaverEditCommitResult::Rejected;
    if (bCancelled)
    {
        Adapter->CancelTimingRowEdit(LaneId, BlockId, RowId);
    }
    else
    {
        CommitResult = Adapter->CommitTimingRowEdit(
            LaneId,
            BlockId,
            RowId,
            FinalStartRatio,
            FinalEndRatio);
    }

    FWeaverTimelinePresentation Presentation = BuildPresentation();
    ValidateCommittedTimingRow(
        Presentation,
        BlockId,
        RowId,
        FinalStartRatio,
        FinalEndRatio,
        CommitResult);
    ApplyPresentation(MoveTemp(Presentation));
}

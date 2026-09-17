#pragma once

#include "CoreMinimal.h"
#include "WeaverTimelineTypes.h"

class SWeaverTimeline;

enum class EWeaverEditCommitResult : uint8
{
    Accepted,
    Rejected
};

struct FWeaverTimelinePresentation
{
    TArray<FWeaverLane> Lanes;
    TArray<FWeaverKey> Keys;
    TArray<FWeaverBlock> Blocks;
};

class IWeaverTimelineEditAdapter
{
public:
    virtual ~IWeaverTimelineEditAdapter() = default;

    virtual void BuildPresentation(FWeaverTimelinePresentation& OutPresentation) = 0;

    virtual void BeginKeyEdit(const FGuid& LaneId, const FGuid& KeyId, double OriginalFrame) {}
    virtual void PreviewKeyEdit(const FGuid& LaneId, const FGuid& KeyId, double PreviewFrame) {}
    virtual EWeaverEditCommitResult CommitKeyEdit(const FGuid& LaneId, const FGuid& KeyId, double FinalFrame)
    {
        return EWeaverEditCommitResult::Rejected;
    }
    virtual void CancelKeyEdit(const FGuid& LaneId, const FGuid& KeyId) {}

    virtual void BeginBlockEdit(
        const FGuid& LaneId,
        const FGuid& BlockId,
        EWeaverBlockEditKind Kind) {}
    virtual void PreviewBlockEdit(
        const FGuid& LaneId,
        const FGuid& BlockId,
        EWeaverBlockEditKind Kind,
        double PreviewStart,
        double PreviewEnd) {}
    virtual EWeaverEditCommitResult CommitBlockEdit(
        const FGuid& LaneId,
        const FGuid& BlockId,
        EWeaverBlockEditKind Kind,
        double FinalStart,
        double FinalEnd)
    {
        return EWeaverEditCommitResult::Rejected;
    }
    virtual void CancelBlockEdit(
        const FGuid& LaneId,
        const FGuid& BlockId,
        EWeaverBlockEditKind Kind) {}

    virtual void BeginTimingRowEdit(
        const FGuid& LaneId,
        const FGuid& BlockId,
        FName RowId,
        float OriginalStartRatio,
        float OriginalEndRatio) {}
    virtual void PreviewTimingRowEdit(
        const FGuid& LaneId,
        const FGuid& BlockId,
        FName RowId,
        float PreviewStartRatio,
        float PreviewEndRatio) {}
    virtual EWeaverEditCommitResult CommitTimingRowEdit(
        const FGuid& LaneId,
        const FGuid& BlockId,
        FName RowId,
        float FinalStartRatio,
        float FinalEndRatio)
    {
        return EWeaverEditCommitResult::Rejected;
    }
    virtual void CancelTimingRowEdit(
        const FGuid& LaneId,
        const FGuid& BlockId,
        FName RowId) {}
};

class FWeaverTimelineEditController final
    : public TSharedFromThis<FWeaverTimelineEditController>
{
public:
    explicit FWeaverTimelineEditController(TSharedRef<IWeaverTimelineEditAdapter> InAdapter);

    void AttachTimeline(const TSharedRef<SWeaverTimeline>& InTimeline);
    void DetachTimeline(const TSharedPtr<SWeaverTimeline>& InTimeline = nullptr);

    void RefreshFromSource();
    void SetBlockExpanded(const FGuid& BlockId, bool bExpanded);

    const FString& GetLastValidationError() const { return LastValidationError; }
    bool HasValidationError() const { return !LastValidationError.IsEmpty(); }

    void HandleKeyEditStarted(FGuid LaneId, FGuid KeyId, double OriginalFrame);
    void HandleKeyEditChanged(FGuid LaneId, FGuid KeyId, double PreviewFrame);
    void HandleKeyEditFinished(FGuid LaneId, FGuid KeyId, double FinalFrame, bool bCancelled);

    void HandleBlockEditStarted(FGuid LaneId, FGuid BlockId, EWeaverBlockEditKind Kind);
    void HandleBlockEditChanged(
        FGuid LaneId,
        FGuid BlockId,
        EWeaverBlockEditKind Kind,
        double PreviewStart,
        double PreviewEnd);
    void HandleBlockEditFinished(
        FGuid LaneId,
        FGuid BlockId,
        double FinalStart,
        double FinalEnd,
        bool bCancelled);

    void HandleTimingRowEditStarted(
        FGuid LaneId,
        FGuid BlockId,
        FName RowId,
        float OriginalStartRatio,
        float OriginalEndRatio);
    void HandleTimingRowEditChanged(
        FGuid LaneId,
        FGuid BlockId,
        FName RowId,
        float PreviewStartRatio,
        float PreviewEndRatio);
    void HandleTimingRowEditFinished(
        FGuid LaneId,
        FGuid BlockId,
        FName RowId,
        float FinalStartRatio,
        float FinalEndRatio,
        bool bCancelled);

private:
    FWeaverTimelinePresentation BuildPresentation();
    void ApplyPresentation(FWeaverTimelinePresentation&& Presentation);
    void SetValidationError(const FString& Error);

    void ValidateCommittedKey(
        const FWeaverTimelinePresentation& Presentation,
        const FGuid& KeyId,
        double FinalFrame,
        EWeaverEditCommitResult CommitResult);
    void ValidateCommittedBlock(
        const FWeaverTimelinePresentation& Presentation,
        const FGuid& BlockId,
        double FinalStart,
        double FinalEnd,
        EWeaverEditCommitResult CommitResult);
    void ValidateCommittedTimingRow(
        const FWeaverTimelinePresentation& Presentation,
        const FGuid& BlockId,
        FName RowId,
        float FinalStartRatio,
        float FinalEndRatio,
        EWeaverEditCommitResult CommitResult);

    TSharedRef<IWeaverTimelineEditAdapter> Adapter;
    TWeakPtr<SWeaverTimeline> Timeline;
    TSet<FGuid> ExpandedBlockIds;

    EWeaverBlockEditKind ActiveBlockEditKind = EWeaverBlockEditKind::Move;
    FString LastValidationError;
};

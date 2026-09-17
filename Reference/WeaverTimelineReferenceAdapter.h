#pragma once

#include "CoreMinimal.h"
#include "WeaverTimelineEditController.h"

class FWeaverTimelineReferenceAdapter final : public IWeaverTimelineEditAdapter
{
public:
    FWeaverTimelineReferenceAdapter();

    void ResetDemoData();
    void RejectNextCommit() { bRejectNextCommit = true; }

    const FWeaverTimelinePresentation& GetState() const { return State; }

    virtual void BuildPresentation(FWeaverTimelinePresentation& OutPresentation) override;
    virtual EWeaverEditCommitResult CommitKeyEdit(
        const FGuid& LaneId,
        const FGuid& KeyId,
        double FinalFrame) override;
    virtual EWeaverEditCommitResult CommitBlockEdit(
        const FGuid& LaneId,
        const FGuid& BlockId,
        EWeaverBlockEditKind Kind,
        double FinalStart,
        double FinalEnd) override;
    virtual EWeaverEditCommitResult CommitTimingRowEdit(
        const FGuid& LaneId,
        const FGuid& BlockId,
        FName RowId,
        float FinalStartRatio,
        float FinalEndRatio) override;

private:
    bool ConsumeRejectNextCommit();

    FWeaverTimelinePresentation State;
    bool bRejectNextCommit = false;
};

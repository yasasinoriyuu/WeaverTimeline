#pragma once

#include "CoreMinimal.h"
#include "WeaverTimelineTypes.h"

class SWeaverTimeline;

struct FWeaverTimelinePresentation
{
    TArray<FWeaverLane> Lanes;
    TArray<FWeaverKey> Keys;
    TArray<FWeaverBlock> Blocks;
};

/** Revision is a monotonic notification generation, not a transactional asset property. */
struct FWeaverSourceContext
{
    FGuid Id;
    uint64 Revision = 0;
    bool operator==(const FWeaverSourceContext& Other) const { return Id == Other.Id && Revision == Other.Revision; }
    bool operator!=(const FWeaverSourceContext& Other) const { return !(*this == Other); }
};

enum class EWeaverEditTarget : uint8 { Key, Block, TimingRow, Command };
enum class EWeaverEditOutcome : uint8 { Applied, NoChange, Rejected, Cancelled };
enum class EWeaverEditEndReason : uint8 { Finished, UserCancelled, SourceChanged, Detached, Hidden };

/** A request, never a promise about final geometry. Commands use CommandId and optional target/frame. */
struct FWeaverEditProposal
{
    EWeaverEditTarget Target = EWeaverEditTarget::Block;
    EWeaverItemType ItemType = EWeaverItemType::Block;
    FGuid LaneId;
    FGuid ItemId;
    FName RowId;
    FName CommandId;
    EWeaverBlockEditKind BlockKind = EWeaverBlockEditKind::Move;
    double Start = 0.0;
    double End = 0.0;
};

struct FWeaverEditSession
{
    FGuid Id;
    FWeaverSourceContext Context;
    FWeaverEditProposal Original;
    FWeaverEditProposal Proposal;
};

struct FWeaverEditResult
{
    EWeaverEditOutcome Outcome = EWeaverEditOutcome::Rejected;
    FText Reason;
};

/** Optional short commit-only scope. Rejected/NoChange must leave authority unchanged. */
class IWeaverEditTransaction
{
public:
    virtual ~IWeaverEditTransaction() = default;
    virtual void Finish(EWeaverEditOutcome Outcome) = 0;
};

DECLARE_MULTICAST_DELEGATE(FWeaverSourceChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FWeaverPresentationReconciled, const FWeaverSourceContext&, const FWeaverEditResult&);

/** All calls/notifications run on the editor/Slate thread. Preview must not mutate authority. */
class IWeaverTimelineEditAdapter
{
public:
    virtual ~IWeaverTimelineEditAdapter() = default;
    virtual FWeaverSourceContext GetContext() const = 0;
    virtual void BuildPresentation(FWeaverTimelinePresentation& OutPresentation) const = 0;
    virtual FWeaverEditResult Commit(const FWeaverEditSession& Session) = 0;
    virtual TUniquePtr<IWeaverEditTransaction> BeginTransaction(const FWeaverEditSession& Session) { return nullptr; }
    virtual void BeginPreview(const FWeaverEditSession& Session) {}
    virtual void UpdatePreview(const FWeaverEditSession& Session) {}
    /** Always paired with BeginPreview, including commit, reject, detach and source replacement.
     * Clean the sink identified by Session.Context/Id; do not restore Original into authority. */
    virtual void EndPreview(const FWeaverEditSession& Session, const FWeaverEditResult& Result, EWeaverEditEndReason Reason) {}
    FWeaverSourceChanged OnSourceChanged;
};

/** Owns proposal -> commit -> EndPreview -> full authority read -> selection reconciliation. */
class FWeaverTimelineEditController : public TSharedFromThis<FWeaverTimelineEditController>
{
public:
    explicit FWeaverTimelineEditController(TSharedRef<IWeaverTimelineEditAdapter> InAdapter);
    ~FWeaverTimelineEditController();
    void AttachTimeline(TSharedRef<SWeaverTimeline> InTimeline);
    void DetachTimeline();
    void RefreshFromSource();
    void Tick();
    bool BeginEdit(const FWeaverEditProposal& Proposal);
    void UpdateEdit(double Start, double End);
    void FinishEdit(double Start, double End, bool bCancelled);
    void CancelEdit(EWeaverEditEndReason Reason = EWeaverEditEndReason::UserCancelled);
    FWeaverEditResult ExecuteCommand(const FWeaverEditProposal& Command);
    void SetBlockExpanded(FGuid BlockId, bool bExpanded);
    bool HasActiveEdit() const { return ActiveSession.IsSet(); }
    const FWeaverTimelinePresentation& GetPresentation() const { return Presentation; }
    const FWeaverEditResult& GetLastResult() const { return LastResult; }
    FWeaverPresentationReconciled OnReconciled;

private:
    void HandleSourceChanged();
    void Complete(bool bCommit, EWeaverEditEndReason Reason);
    bool ReadOriginal(FWeaverEditProposal& Proposal) const;
    void ApplyPresentation();
    TSharedRef<IWeaverTimelineEditAdapter> Adapter;
    TWeakPtr<SWeaverTimeline> Timeline;
    FDelegateHandle SourceChangedHandle;
    FWeaverTimelinePresentation Presentation;
    FWeaverSourceContext PresentedContext;
    TOptional<FWeaverEditSession> ActiveSession;
    TMap<FGuid, bool> ExpansionOverrides;
    FWeaverEditResult LastResult;
    bool bDispatching = false;
    bool bRefreshPending = false;
    bool bSourceInvalidated = false;
    bool bAttached = false;
    TOptional<EWeaverEditEndReason> PendingCancelReason;
};

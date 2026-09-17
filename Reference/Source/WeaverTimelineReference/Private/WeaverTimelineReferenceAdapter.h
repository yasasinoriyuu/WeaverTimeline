#pragma once

#include "WeaverTimelineEditController.h"
#include "WeaverReferenceDocument.h"
#include "UObject/StrongObjectPtr.h"

enum class EWeaverReferencePolicy : uint8 { Normal, Snap, Clamp, Reject, Cascade };

class FWeaverTimelineReferenceAdapter : public IWeaverTimelineEditAdapter
{
public:
    FWeaverTimelineReferenceAdapter();
    explicit FWeaverTimelineReferenceAdapter(UWeaverReferenceDocument& InDocument);
    virtual ~FWeaverTimelineReferenceAdapter();
    virtual FWeaverSourceContext GetContext() const override;
    virtual void BuildPresentation(FWeaverTimelinePresentation& Out) const override;
    virtual FWeaverEditResult Commit(const FWeaverEditSession& Session) override;
    virtual TUniquePtr<IWeaverEditTransaction> BeginTransaction(const FWeaverEditSession& Session) override;
    virtual void BeginPreview(const FWeaverEditSession& Session) override;
    virtual void UpdatePreview(const FWeaverEditSession& Session) override;
    virtual void EndPreview(const FWeaverEditSession& Session, const FWeaverEditResult& Result, EWeaverEditEndReason Reason) override;
    void ReplaceDocument();
    void NotifySourceChanged();
    UWeaverReferenceDocument& GetDocument() const { return *Document; }
    EWeaverReferencePolicy Policy = EWeaverReferencePolicy::Normal;
    int32 CommitCount = 0;
    int32 BeginPreviewCount = 0;
    int32 EndPreviewCount = 0;
    TMap<FGuid, FWeaverEditSession> Previews;
    TFunction<void()> DuringCommit; // Reference fault injection for reentrancy regression.
private:
    TStrongObjectPtr<UWeaverReferenceDocument> Document;
    FDelegateHandle DocumentChangedHandle;
    uint64 Revision = 0;
};

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WeaverReferenceDocument.generated.h"

USTRUCT()
struct FWeaverReferenceItem
{
    GENERATED_BODY()
    UPROPERTY() FGuid Id;
    UPROPERTY() bool bKey = false;
    UPROPERTY() double Start = 20;
    UPROPERTY() double End = 60;
    UPROPERTY() float TimingStart = 0.2f;
    UPROPERTY() float TimingEnd = 0.8f;
    bool operator==(const FWeaverReferenceItem& Other) const
    {
        return Id == Other.Id && bKey == Other.bKey && Start == Other.Start && End == Other.End
            && TimingStart == Other.TimingStart && TimingEnd == Other.TimingEnd;
    }
};

/** Business-neutral authoritative UObject. UI presentation and transient previews are not serialized. */
UCLASS()
class UWeaverReferenceDocument : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TArray<FWeaverReferenceItem> Items;
    UPROPERTY() FGuid LaneId;
    FGuid ContextId = FGuid::NewGuid();
    FSimpleMulticastDelegate OnChanged;
    virtual void PostEditUndo() override;
};

#pragma once

#include "CoreMinimal.h"

enum class EWeaverItemType : uint8
{
    None,
    Key,
    Block
};

enum class EWeaverBlockEditKind : uint8
{
    Move,
    ResizeStart,
    ResizeEnd
};

struct FWeaverLaneHeaderAction
{
    FName ActionId;
    FText Label;
    FName IconName;
    FText Tooltip;
    bool bToggled = false;
    bool bEnabled = true;
};

struct FWeaverTimingRow
{
    FName RowId;
    FText Label;
    float StartRatio = 0.0f;
    float EndRatio = 1.0f;
    bool bEnabled = true;
};

struct FWeaverLane
{
    FGuid LaneId;
    FText Label;
    FLinearColor AccentColor = FLinearColor(0.18f, 0.48f, 0.85f, 1.0f);
    bool bEnabled = true;
    TArray<FWeaverLaneHeaderAction> HeaderActions;
};

struct FWeaverKey
{
    FGuid KeyId;
    FGuid LaneId;
    double Frame = 0.0;
    FLinearColor Color = FLinearColor::White;
    bool bEnabled = true;
};

struct FWeaverBlock
{
    FGuid BlockId;
    FGuid LaneId;
    double StartFrame = 0.0;
    double EndFrame = 1.0;
    FText Label;
    FLinearColor Color = FLinearColor(0.2f, 0.45f, 0.8f, 0.8f);
    bool bEnabled = true;
    bool bResizable = true;
    bool bExpanded = false;
    TArray<FWeaverTimingRow> TimingRows;
};

struct FWeaverSelection
{
    EWeaverItemType Type = EWeaverItemType::None;
    FGuid LaneId;
    FGuid ItemId;

    bool IsValid() const
    {
        return Type != EWeaverItemType::None && LaneId.IsValid() && ItemId.IsValid();
    }

    void Reset()
    {
        Type = EWeaverItemType::None;
        LaneId.Invalidate();
        ItemId.Invalidate();
    }

    bool operator==(const FWeaverSelection& Other) const
    {
        return Type == Other.Type && LaneId == Other.LaneId && ItemId == Other.ItemId;
    }

    bool operator!=(const FWeaverSelection& Other) const
    {
        return !(*this == Other);
    }
};

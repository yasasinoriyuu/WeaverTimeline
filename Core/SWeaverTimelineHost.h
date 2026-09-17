#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Generic resizable/collapsible host for an orchestration widget.
 * Extracted from CAK's proven bottom-of-Level-Viewport planner shell.
 *
 * This widget owns presentation mechanics only. It does not know what the
 * hosted content means and it does not register any global tab/menu/style IDs.
 */
class SWeaverTimelineHost final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWeaverTimelineHost)
        : _ExpandedHeight(300.0f)
        , _CollapsedLabel(FText::FromString(TEXT("Timeline")))
    {}
        SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_ARGUMENT(float, ExpandedHeight)
        SLATE_ARGUMENT(FText, CollapsedLabel)
        SLATE_EVENT(FSimpleDelegate, OnDeactivated)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual FReply OnMouseButtonDown(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseButtonUp(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseMove(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override;
    virtual FCursorReply OnCursorQuery(
        const FGeometry& MyGeometry,
        const FPointerEvent& CursorEvent) const override;
    virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
    FReply ToggleCollapsed();
    FOptionalSize GetDesiredPanelHeight() const;
    bool IsInResizeZone(const FGeometry& Geometry, const FVector2D& ScreenPosition) const;
    FText GetCollapsedButtonText() const;

    float ExpandedHeight = 300.0f;
    float ResizeStartHeight = 300.0f;
    float ResizeStartScreenY = 0.0f;
    bool bCollapsed = false;
    bool bResizing = false;
    FText CollapsedLabel;
    FSimpleDelegate OnDeactivated;
};

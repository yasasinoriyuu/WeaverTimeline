#include "SWeaverTimelineHost.h"

#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
constexpr float ResizeZoneHeight = 7.0f;
constexpr float CollapsedHeight = 24.0f;
constexpr float MinimumExpandedHeight = 150.0f;
constexpr float MaximumExpandedHeight = 520.0f;
}

void SWeaverTimelineHost::Construct(const FArguments& InArgs)
{
    ExpandedHeight = FMath::Clamp(
        InArgs._ExpandedHeight,
        MinimumExpandedHeight,
        MaximumExpandedHeight);
    ResizeStartHeight = ExpandedHeight;
    CollapsedLabel = InArgs._CollapsedLabel;

    ChildSlot
    [
        SNew(SBox)
        .HeightOverride(this, &SWeaverTimelineHost::GetDesiredPanelHeight)
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            [
                SNew(SVerticalBox)
                .Visibility_Lambda([this]()
                {
                    return bCollapsed ? EVisibility::Collapsed : EVisibility::Visible;
                })
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SBox)
                    .HeightOverride(ResizeZoneHeight)
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(6.0f, 0.0f, 6.0f, 4.0f)
                [
                    InArgs._Content.Widget
                ]
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Top)
            .Padding(0.0f, ResizeZoneHeight, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Visibility_Lambda([this]()
                {
                    return bCollapsed ? EVisibility::Collapsed : EVisibility::Visible;
                })
                .ContentPadding(FMargin(6.0f, 0.0f))
                .ToolTipText(FText::FromString(TEXT("折叠编排器")))
                .OnClicked(this, &SWeaverTimelineHost::ToggleCollapsed)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("﹀")))
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                ]
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Bottom)
            [
                SNew(SButton)
                .Visibility_Lambda([this]()
                {
                    return bCollapsed ? EVisibility::Visible : EVisibility::Collapsed;
                })
                .ContentPadding(FMargin(7.0f, 1.0f))
                .ToolTipText(FText::FromString(TEXT("展开编排器")))
                .OnClicked(this, &SWeaverTimelineHost::ToggleCollapsed)
                [
                    SNew(STextBlock)
                    .Text(this, &SWeaverTimelineHost::GetCollapsedButtonText)
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                ]
            ]
        ]
    ];
}

FOptionalSize SWeaverTimelineHost::GetDesiredPanelHeight() const
{
    return FOptionalSize(bCollapsed ? CollapsedHeight : ExpandedHeight);
}

bool SWeaverTimelineHost::IsInResizeZone(
    const FGeometry& Geometry,
    const FVector2D& ScreenPosition) const
{
    if (bCollapsed)
    {
        return false;
    }

    const FVector2D Local = Geometry.AbsoluteToLocal(ScreenPosition);
    return Local.Y >= 0.0f && Local.Y <= ResizeZoneHeight;
}

FReply SWeaverTimelineHost::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
        && IsInResizeZone(MyGeometry, MouseEvent.GetScreenSpacePosition()))
    {
        bResizing = true;
        ResizeStartHeight = ExpandedHeight;
        ResizeStartScreenY = MouseEvent.GetScreenSpacePosition().Y;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    return FReply::Unhandled();
}

FReply SWeaverTimelineHost::OnMouseButtonUp(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bResizing)
    {
        bResizing = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    return FReply::Unhandled();
}

FReply SWeaverTimelineHost::OnMouseMove(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (!bResizing || !HasMouseCapture())
    {
        return FReply::Unhandled();
    }

    const float DeltaY = MouseEvent.GetScreenSpacePosition().Y - ResizeStartScreenY;
    ExpandedHeight = FMath::Clamp(
        ResizeStartHeight - DeltaY,
        MinimumExpandedHeight,
        MaximumExpandedHeight);
    Invalidate(EInvalidateWidgetReason::Layout);
    return FReply::Handled();
}

FCursorReply SWeaverTimelineHost::OnCursorQuery(
    const FGeometry& MyGeometry,
    const FPointerEvent& CursorEvent) const
{
    return IsInResizeZone(MyGeometry, CursorEvent.GetScreenSpacePosition())
        ? FCursorReply::Cursor(EMouseCursor::ResizeUpDown)
        : FCursorReply::Unhandled();
}

void SWeaverTimelineHost::OnMouseCaptureLost(
    const FCaptureLostEvent& CaptureLostEvent)
{
    bResizing = false;
    SCompoundWidget::OnMouseCaptureLost(CaptureLostEvent);
}

FReply SWeaverTimelineHost::ToggleCollapsed()
{
    bCollapsed = !bCollapsed;
    Invalidate(EInvalidateWidgetReason::Layout);
    return FReply::Handled();
}

FText SWeaverTimelineHost::GetCollapsedButtonText() const
{
    return FText::Format(
        FText::FromString(TEXT("{0} ︿")),
        CollapsedLabel);
}

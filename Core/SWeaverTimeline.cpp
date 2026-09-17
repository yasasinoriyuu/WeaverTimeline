#include "SWeaverTimeline.h"

#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

namespace
{
constexpr float KeyRadius = 5.0f;
constexpr float BlockVerticalPadding = 4.0f;
constexpr float LabelTextPadding = 10.0f;
constexpr float BlockResizeHandleWidth = 7.0f;
constexpr float RightPanThreshold = 4.0f;
constexpr float PrimaryDragThreshold = 4.0f;
constexpr float TimingRowHeight = 18.0f;
constexpr float TimingRowHandleWidth = 5.0f;
constexpr float ExpansionToggleInset = 6.0f;
constexpr float ExpansionToggleWidth = 10.0f;
constexpr float ExpansionToggleHeight = 16.0f;
constexpr double MinBlockDurationFrames = 0.001;
}

FWeaverSelection SWeaverTimeline::FHitResult::ToSelection() const
{
    FWeaverSelection Result;
    if (KeyId.IsValid())
    {
        Result.Type = EWeaverItemType::Key;
        Result.LaneId = LaneId;
        Result.ItemId = KeyId;
    }
    else if (BlockId.IsValid())
    {
        Result.Type = EWeaverItemType::Block;
        Result.LaneId = LaneId;
        Result.ItemId = BlockId;
    }
    return Result;
}

void SWeaverTimeline::Construct(const FArguments& InArgs)
{
    CurrentFrame = InArgs._CurrentFrame;
    RulerHeight = InArgs._RulerHeight;
    LaneHeight = InArgs._LaneHeight;
    LabelWidth = InArgs._LabelWidth;
    bDeferDeleteSelectionToSource = InArgs._DeferDeleteSelectionToSource;
    LeftPadding = LabelWidth;

    OnFrameChanged = InArgs._OnFrameChanged;
    OnSelectionChanged = InArgs._OnSelectionChanged;
    OnKeyEditStarted = InArgs._OnKeyEditStarted;
    OnKeyEditChanged = InArgs._OnKeyEditChanged;
    OnKeyEditFinished = InArgs._OnKeyEditFinished;
    OnBlockEditStarted = InArgs._OnBlockEditStarted;
    OnBlockEditChanged = InArgs._OnBlockEditChanged;
    OnBlockEditFinished = InArgs._OnBlockEditFinished;
    OnDeleteRequested = InArgs._OnDeleteRequested;
    OnContextRequested = InArgs._OnContextRequested;
    OnViewRangeChanged = InArgs._OnViewRangeChanged;
    OnLaneHeaderActionRequested = InArgs._OnLaneHeaderActionRequested;
    OnBlockEndpointClicked = InArgs._OnBlockEndpointClicked;
    OnLaneContextRequested = InArgs._OnLaneContextRequested;
    OnBlockExpansionChanged = InArgs._OnBlockExpansionChanged;
    OnTimingRowEditStarted = InArgs._OnTimingRowEditStarted;
    OnTimingRowEditChanged = InArgs._OnTimingRowEditChanged;
    OnTimingRowEditFinished = InArgs._OnTimingRowEditFinished;
}

void SWeaverTimeline::SetLanes(TArray<FWeaverLane> InLanes)
{
    Lanes = MoveTemp(InLanes);
    Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

void SWeaverTimeline::SetKeys(TArray<FWeaverKey> InKeys)
{
    Keys = MoveTemp(InKeys);
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SWeaverTimeline::SetBlocks(TArray<FWeaverBlock> InBlocks)
{
    Blocks = MoveTemp(InBlocks);
    Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

void SWeaverTimeline::SetSelection(const FWeaverSelection& InSelection)
{
    ApplySelection(InSelection, false);
}

void SWeaverTimeline::ClearSelection(bool bNotify)
{
    FWeaverSelection Empty;
    ApplySelection(Empty, bNotify);
}

void SWeaverTimeline::UnbindCallbacks()
{
    OnFrameChanged.Unbind();
    OnSelectionChanged.Unbind();
    OnKeyEditStarted.Unbind(); OnKeyEditChanged.Unbind(); OnKeyEditFinished.Unbind();
    OnBlockEditStarted.Unbind(); OnBlockEditChanged.Unbind(); OnBlockEditFinished.Unbind();
    OnTimingRowEditStarted.Unbind(); OnTimingRowEditChanged.Unbind(); OnTimingRowEditFinished.Unbind();
    OnDeleteRequested.Unbind(); OnContextRequested.Unbind(); OnViewRangeChanged.Unbind();
    OnLaneHeaderActionRequested.Unbind(); OnBlockEndpointClicked.Unbind();
    OnLaneContextRequested.Unbind(); OnBlockExpansionChanged.Unbind();
    CurrentFrame = 0.0;
}

void SWeaverTimeline::SetExternalViewRange(const double StartFrame, const double EndFrame)
{
    if (!FMath::IsFinite(StartFrame) || !FMath::IsFinite(EndFrame) || EndFrame <= StartFrame)
    {
        return;
    }

    bExternalViewRange = true;
    ViewStartFrame = StartFrame;
    ViewEndFrame = EndFrame;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SWeaverTimeline::ClearExternalViewRange()
{
    bExternalViewRange = false;
}

void SWeaverTimeline::SetHorizontalPadding(const float Left, const float Right)
{
    LeftPadding = FMath::Max(LabelWidth, Left);
    RightPadding = FMath::Max(0.0f, Right);
    Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SWeaverTimeline::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
    float Height = RulerHeight;
    for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
    {
        Height += LaneHeightForIndex(LaneIndex);
    }
    return FVector2D(900.0f, Height + (Lanes.Num() == 0 ? LaneHeight : 0.0f));
}

int32 SWeaverTimeline::FindLaneIndex(const FGuid& LaneId) const
{
    for (int32 Index = 0; Index < Lanes.Num(); ++Index)
    {
        if (Lanes[Index].LaneId == LaneId)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

const FWeaverKey* SWeaverTimeline::FindKey(const FGuid& KeyId) const
{
    return Keys.FindByPredicate([&KeyId](const FWeaverKey& Key)
    {
        return Key.KeyId == KeyId;
    });
}

const FWeaverBlock* SWeaverTimeline::FindBlock(const FGuid& BlockId) const
{
    return Blocks.FindByPredicate([&BlockId](const FWeaverBlock& Block)
    {
        return Block.BlockId == BlockId;
    });
}

float SWeaverTimeline::TrackLeft() const
{
    return LeftPadding;
}

float SWeaverTimeline::TrackRight(const FGeometry& Geometry) const
{
    return FMath::Max(TrackLeft() + 1.0f, Geometry.GetLocalSize().X - RightPadding);
}

float SWeaverTimeline::LaneTop(const int32 LaneIndex) const
{
    float Top = RulerHeight;
    for (int32 Index = 0; Index < LaneIndex; ++Index)
    {
        Top += LaneHeightForIndex(Index);
    }
    return Top;
}

void SWeaverTimeline::SetPresentation(TArray<FWeaverLane> InLanes, TArray<FWeaverKey> InKeys, TArray<FWeaverBlock> InBlocks)
{
    Lanes = MoveTemp(InLanes);
    Keys = MoveTemp(InKeys);
    Blocks = MoveTemp(InBlocks);
    if (Selection.IsValid())
    {
        FWeaverSelection Reconciled = Selection;
        bool bFound = false;
        if (Selection.Type == EWeaverItemType::Key)
        {
            if (const auto* Key = FindKey(Selection.ItemId)) { Reconciled.LaneId = Key->LaneId; bFound = true; }
        }
        else if (const auto* Block = FindBlock(Selection.ItemId)) { Reconciled.LaneId = Block->LaneId; bFound = true; }
        if (!bFound || FindLaneIndex(Reconciled.LaneId) == INDEX_NONE) { Reconciled.Reset(); }
        ApplySelection(Reconciled, true);
    }
    HoverSelection.Reset();
    Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

void SWeaverTimeline::CancelInteraction()
{
    FinishPrimaryInteraction(true);
    ResetRightMouseState();
    if (FSlateApplication::IsInitialized() && HasMouseCapture())
    {
        FSlateApplication::Get().GetCursorUser()->ReleaseCursorCapture();
    }
}

int32 SWeaverTimeline::ExpandedTimingRowCount(const int32 LaneIndex) const
{
    if (!Lanes.IsValidIndex(LaneIndex))
    {
        return 0;
    }

    int32 MaxRows = 0;
    const FGuid LaneId = Lanes[LaneIndex].LaneId;
    for (const FWeaverBlock& Block : Blocks)
    {
        if (Block.LaneId == LaneId && Block.bExpanded)
        {
            MaxRows = FMath::Max(MaxRows, Block.TimingRows.Num());
        }
    }
    return MaxRows;
}

float SWeaverTimeline::LaneHeightForIndex(const int32 LaneIndex) const
{
    return LaneHeight + ExpandedTimingRowCount(LaneIndex) * TimingRowHeight;
}

float SWeaverTimeline::TimingRowTop(const int32 LaneIndex, const int32 RowIndex) const
{
    return LaneTop(LaneIndex) + LaneHeight + RowIndex * TimingRowHeight;
}

SWeaverTimeline::FExpansionToggleGeometry SWeaverTimeline::GetExpansionToggleGeometry(
    const float BlockX0,
    const float BlockX1,
    const float BlockTop,
    const float BlockHeight) const
{
    FExpansionToggleGeometry Result;
    const float Left = BlockX0 + BlockResizeHandleWidth + ExpansionToggleInset;
    const float Right = FMath::Min(
        Left + ExpansionToggleWidth,
        BlockX1 - BlockResizeHandleWidth - 2.0f);
    const float Top = BlockTop + FMath::Max(0.0f, (BlockHeight - ExpansionToggleHeight) * 0.5f);
    const float Bottom = FMath::Min(Top + ExpansionToggleHeight, BlockTop + BlockHeight);

    if (Right <= Left || Bottom <= Top)
    {
        return Result;
    }

    Result.Left = Left;
    Result.Top = Top;
    Result.Right = Right;
    Result.Bottom = Bottom;
    return Result;
}

float SWeaverTimeline::FrameToLocalX(const FGeometry& Geometry, const double Frame) const
{
    const double Span = FMath::Max(0.001, ViewEndFrame - ViewStartFrame);
    const float Width = FMath::Max(1.0f, TrackRight(Geometry) - TrackLeft());
    return TrackLeft() + static_cast<float>((Frame - ViewStartFrame) / Span) * Width;
}

double SWeaverTimeline::LocalXToFrame(const FGeometry& Geometry, const float X, const bool bClampToView) const
{
    const float Width = FMath::Max(1.0f, TrackRight(Geometry) - TrackLeft());
    double Alpha = static_cast<double>((X - TrackLeft()) / Width);
    if (bClampToView)
    {
        Alpha = FMath::Clamp(Alpha, 0.0, 1.0);
    }
    return ViewStartFrame + Alpha * (ViewEndFrame - ViewStartFrame);
}

double SWeaverTimeline::PixelsToFrames(const FGeometry& Geometry, const float DeltaX) const
{
    const float Width = FMath::Max(1.0f, TrackRight(Geometry) - TrackLeft());
    const double Span = FMath::Max(0.001, ViewEndFrame - ViewStartFrame);
    return static_cast<double>(DeltaX / Width) * Span;
}

double SWeaverTimeline::ChooseTickStep(const double VisibleFrames, const float TrackWidth) const
{
    const double DesiredTickCount = FMath::Max(2.0, static_cast<double>(TrackWidth) / 90.0);
    const double Raw = FMath::Max(0.001, VisibleFrames / DesiredTickCount);
    const double Power = FMath::Pow(10.0, FMath::FloorToDouble(FMath::LogX(10.0, Raw)));
    const double Normalized = Raw / Power;
    const double Nice = Normalized <= 1.0 ? 1.0 : (Normalized <= 2.0 ? 2.0 : (Normalized <= 5.0 ? 5.0 : 10.0));
    return Nice * Power;
}

void SWeaverTimeline::DrawRuler(
    FSlateWindowElementList& OutDrawElements,
    const int32 LayerId,
    const FGeometry& Geometry) const
{
    const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
    const float Left = TrackLeft();
    const float Right = TrackRight(Geometry);
    const double Span = FMath::Max(0.001, ViewEndFrame - ViewStartFrame);
    const double Step = ChooseTickStep(Span, Right - Left);
    const double First = FMath::CeilToDouble(ViewStartFrame / Step) * Step;
    const FSlateFontInfo Font = FAppStyle::GetFontStyle("SmallFont");

    for (double Frame = First; Frame <= ViewEndFrame + Step * 0.25; Frame += Step)
    {
        const float X = FrameToLocalX(Geometry, Frame);
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId,
            Geometry.ToPaintGeometry(
                FVector2f(1.0f, RulerHeight),
                FSlateLayoutTransform(FVector2f(X, 0.0f))),
            WhiteBrush,
            ESlateDrawEffect::None,
            FLinearColor(1, 1, 1, 0.12f));

        FSlateDrawElement::MakeText(
            OutDrawElements,
            LayerId + 1,
            Geometry.ToPaintGeometry(
                FVector2f(60.0f, RulerHeight - 2.0f),
                FSlateLayoutTransform(FVector2f(X + 4.0f, 2.0f))),
            FText::AsNumber(FMath::RoundToInt(Frame)),
            Font,
            ESlateDrawEffect::None,
            FLinearColor(0.72f, 0.74f, 0.78f, 1.0f));
    }
}

void SWeaverTimeline::DrawLaneHeaderActions(
    FSlateWindowElementList& OutDrawElements,
    const int32 LayerId,
    const FGeometry& Geometry,
    const int32 LaneIndex) const
{
    if (!Lanes.IsValidIndex(LaneIndex))
    {
        return;
    }

    const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
    const FSlateFontInfo Font = FAppStyle::GetFontStyle("SmallFont");
    const FWeaverLane& Lane = Lanes[LaneIndex];
    const float Top = LaneTop(LaneIndex);
    const float ActionWidth = 18.0f;
    const float StartX = FMath::Max(4.0f, LeftPadding - ActionWidth * Lane.HeaderActions.Num() - 4.0f);

    for (int32 ActionIndex = 0; ActionIndex < Lane.HeaderActions.Num(); ++ActionIndex)
    {
        const FWeaverLaneHeaderAction& Action = Lane.HeaderActions[ActionIndex];
        const float X = StartX + ActionIndex * ActionWidth;
        const FLinearColor Fill = !Action.bEnabled
            ? FLinearColor(1, 1, 1, 0.04f)
            : (Action.bToggled ? Lane.AccentColor.CopyWithNewOpacity(0.55f) : FLinearColor(1, 1, 1, 0.10f));
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId,
            Geometry.ToPaintGeometry(
                FVector2f(ActionWidth - 2.0f, 18.0f),
                FSlateLayoutTransform(FVector2f(X, Top + 7.0f))),
            WhiteBrush,
            ESlateDrawEffect::None,
            Fill);

        const FText ActionText = !Action.Label.IsEmpty()
            ? Action.Label
            : (Action.IconName.IsNone() ? FText::FromName(Action.ActionId) : FText::FromName(Action.IconName));
        FSlateDrawElement::MakeText(
            OutDrawElements,
            LayerId + 1,
            Geometry.ToPaintGeometry(
                FVector2f(ActionWidth - 2.0f, 16.0f),
                FSlateLayoutTransform(FVector2f(X, Top + 8.0f))),
            ActionText,
            Font,
            ESlateDrawEffect::None,
            Action.bEnabled ? FLinearColor::White : FLinearColor(1, 1, 1, 0.3f));
    }
}

void SWeaverTimeline::DrawTimingRows(
    FSlateWindowElementList& OutDrawElements,
    const int32 LayerId,
    const FGeometry& Geometry,
    const FWeaverBlock& Block,
    const int32 LaneIndex,
    const float BlockX0,
    const float BlockX1) const
{
    if (!Block.bExpanded || Block.TimingRows.Num() == 0 || !Lanes.IsValidIndex(LaneIndex))
    {
        return;
    }

    const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
    const FSlateFontInfo Font = FAppStyle::GetFontStyle("SmallFont");
    const float Width = FMath::Max(1.0f, BlockX1 - BlockX0);
    for (int32 RowIndex = 0; RowIndex < Block.TimingRows.Num(); ++RowIndex)
    {
        const FWeaverTimingRow& Row = Block.TimingRows[RowIndex];
        float StartRatio = FMath::Clamp(Row.StartRatio, 0.0f, 1.0f);
        float EndRatio = FMath::Clamp(Row.EndRatio, StartRatio, 1.0f);
        if (DragMode == EDragMode::TimingRow && DragTimingBlockId == Block.BlockId && DragTimingRowId == Row.RowId)
        {
            StartRatio = DragPreviewTimingStart;
            EndRatio = DragPreviewTimingEnd;
        }

        const float Top = TimingRowTop(LaneIndex, RowIndex) + 2.0f;
        FSlateDrawElement::MakeText(
            OutDrawElements,
            LayerId,
            Geometry.ToPaintGeometry(
                FVector2f(FMath::Max(20.0f, LeftPadding - 18.0f), TimingRowHeight - 2.0f),
                FSlateLayoutTransform(FVector2f(LabelTextPadding + 8.0f, Top))),
            Row.Label,
            Font,
            ESlateDrawEffect::None,
            Row.bEnabled ? FLinearColor(0.72f, 0.74f, 0.78f, 1.0f) : FLinearColor(1, 1, 1, 0.3f));

        const float X0 = BlockX0 + Width * StartRatio;
        const float X1 = BlockX0 + Width * EndRatio;
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId,
            Geometry.ToPaintGeometry(
                FVector2f(FMath::Max(1.0f, X1 - X0), 8.0f),
                FSlateLayoutTransform(FVector2f(X0, Top + 4.0f))),
            WhiteBrush,
            ESlateDrawEffect::None,
            Block.Color.CopyWithNewOpacity(Row.bEnabled ? 0.55f : 0.2f));

        if (Row.bEnabled)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 1,
                Geometry.ToPaintGeometry(
                    FVector2f(2.0f, 12.0f),
                    FSlateLayoutTransform(FVector2f(X0 - 1.0f, Top + 2.0f))),
                WhiteBrush,
                ESlateDrawEffect::None,
                FLinearColor::White);
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 1,
                Geometry.ToPaintGeometry(
                    FVector2f(2.0f, 12.0f),
                    FSlateLayoutTransform(FVector2f(X1 - 1.0f, Top + 2.0f))),
                WhiteBrush,
                ESlateDrawEffect::None,
                FLinearColor::White);
        }
    }
}

void SWeaverTimeline::DrawDiamond(
    FSlateWindowElementList& OutDrawElements,
    const int32 LayerId,
    const FGeometry& Geometry,
    const FVector2D& Center,
    const FLinearColor& Color,
    const bool bSelected,
    const bool bHovered,
    const bool bEnabled) const
{
    const float Radius = bSelected ? KeyRadius + 2.0f : (bHovered ? KeyRadius + 1.0f : KeyRadius);
    const FLinearColor DrawColor = bEnabled
        ? (bSelected ? FLinearColor::White : Color)
        : Color.CopyWithNewOpacity(0.35f);

    const TArray<FVector2f> Points =
    {
        FVector2f(Center.X, Center.Y - Radius),
        FVector2f(Center.X + Radius, Center.Y),
        FVector2f(Center.X, Center.Y + Radius),
        FVector2f(Center.X - Radius, Center.Y),
        FVector2f(Center.X, Center.Y - Radius)
    };

    FSlateDrawElement::MakeLines(
        OutDrawElements,
        LayerId,
        Geometry.ToPaintGeometry(),
        Points,
        ESlateDrawEffect::None,
        DrawColor,
        true,
        bSelected ? 2.0f : 1.5f);
}

int32 SWeaverTimeline::OnPaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    const int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    const bool bParentEnabled) const
{
    const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
    const FVector2D Size = AllottedGeometry.GetLocalSize();

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        LayerId,
        AllottedGeometry.ToPaintGeometry(),
        WhiteBrush,
        ESlateDrawEffect::None,
        FLinearColor(0.028f, 0.032f, 0.04f, 1.0f));

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        LayerId + 1,
        AllottedGeometry.ToPaintGeometry(
            FVector2f(LeftPadding, Size.Y),
            FSlateLayoutTransform()),
        WhiteBrush,
        ESlateDrawEffect::None,
        FLinearColor(0.05f, 0.055f, 0.07f, 1.0f));

    DrawRuler(OutDrawElements, LayerId + 2, AllottedGeometry);

    const FSlateFontInfo LabelFont = FAppStyle::GetFontStyle("NormalFont");
    for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
    {
        const FWeaverLane& Lane = Lanes[LaneIndex];
        const float Top = LaneTop(LaneIndex);
        const float LaneDrawHeight = LaneHeightForIndex(LaneIndex);
        if (LaneIndex % 2 == 1)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 1,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f(Size.X, LaneDrawHeight),
                    FSlateLayoutTransform(FVector2f(0.0f, Top))),
                WhiteBrush,
                ESlateDrawEffect::None,
                FLinearColor(1, 1, 1, 0.018f));
        }

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId + 3,
            AllottedGeometry.ToPaintGeometry(
                FVector2f(3.0f, LaneHeight - 8.0f),
                FSlateLayoutTransform(FVector2f(4.0f, Top + 4.0f))),
            WhiteBrush,
            ESlateDrawEffect::None,
            Lane.bEnabled ? Lane.AccentColor : Lane.AccentColor.CopyWithNewOpacity(0.25f));

        FSlateDrawElement::MakeText(
            OutDrawElements,
            LayerId + 4,
            AllottedGeometry.ToPaintGeometry(
                FVector2f(FMath::Max(20.0f, LeftPadding - 18.0f), 22.0f),
                FSlateLayoutTransform(FVector2f(LabelTextPadding, Top + 8.0f))),
            Lane.Label,
            LabelFont,
            ESlateDrawEffect::None,
            Lane.bEnabled ? FLinearColor::White : FLinearColor(1, 1, 1, 0.35f));

        DrawLaneHeaderActions(OutDrawElements, LayerId + 5, AllottedGeometry, LaneIndex);
    }

    for (const FWeaverBlock& Block : Blocks)
    {
        const int32 LaneIndex = FindLaneIndex(Block.LaneId);
        if (!Lanes.IsValidIndex(LaneIndex))
        {
            continue;
        }

        double DrawStart = Block.StartFrame;
        double DrawEnd = Block.EndFrame;
        if (DragMode == EDragMode::Block && DragItemId == Block.BlockId)
        {
            DrawStart = DragPreviewBlockStart;
            DrawEnd = DragPreviewBlockEnd;
        }

        const float X0 = FrameToLocalX(AllottedGeometry, FMath::Min(DrawStart, DrawEnd));
        const float X1 = FrameToLocalX(AllottedGeometry, FMath::Max(DrawStart, DrawEnd));
        const float Top = LaneTop(LaneIndex) + BlockVerticalPadding;
        const float Width = FMath::Max(1.0f, X1 - X0);
        const float Height = FMath::Max(1.0f, LaneHeight - BlockVerticalPadding * 2.0f);
        const bool bSelected = Selection.Type == EWeaverItemType::Block && Selection.ItemId == Block.BlockId;
        const bool bHovered = HoverSelection.Type == EWeaverItemType::Block && HoverSelection.ItemId == Block.BlockId;

        FLinearColor BlockColor = Block.Color;
        if (!Block.bEnabled)
        {
            BlockColor = BlockColor.CopyWithNewOpacity(0.28f);
        }
        else if (bSelected)
        {
            BlockColor = BlockColor.CopyWithNewOpacity(1.0f);
        }
        else if (bHovered)
        {
            BlockColor = BlockColor.CopyWithNewOpacity(FMath::Min(1.0f, BlockColor.A + 0.15f));
        }

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId + 5,
            AllottedGeometry.ToPaintGeometry(
                FVector2f(Width, Height),
                FSlateLayoutTransform(FVector2f(X0, Top))),
            WhiteBrush,
            ESlateDrawEffect::None,
            BlockColor);

        if (Block.bResizable && Block.bEnabled && (bSelected || bHovered))
        {
            const FLinearColor HandleColor(1.0f, 1.0f, 1.0f, bSelected ? 0.8f : 0.45f);
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 6,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f(2.0f, Height),
                    FSlateLayoutTransform(FVector2f(X0 + 1.0f, Top))),
                WhiteBrush,
                ESlateDrawEffect::None,
                HandleColor);
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 6,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f(2.0f, Height),
                    FSlateLayoutTransform(FVector2f(FMath::Max(X0 + 1.0f, X1 - 3.0f), Top))),
                WhiteBrush,
                ESlateDrawEffect::None,
                HandleColor);
        }

        const FExpansionToggleGeometry ToggleGeometry = GetExpansionToggleGeometry(X0, X1, Top, Height);
        const float LabelLeft = ToggleGeometry.IsValid() ? ToggleGeometry.Right + 4.0f : X0 + 6.0f;
        const float BlockLabelWidth = X1 - LabelLeft - 4.0f;
        if (!Block.Label.IsEmpty() && BlockLabelWidth > 20.0f)
        {
            FSlateDrawElement::MakeText(
                OutDrawElements,
                LayerId + 7,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f(BlockLabelWidth, Height - 4.0f),
                    FSlateLayoutTransform(FVector2f(LabelLeft, Top + 4.0f))),
                Block.Label,
                FAppStyle::GetFontStyle("SmallFont"),
                ESlateDrawEffect::None,
            Block.bEnabled ? FLinearColor::White : FLinearColor(1, 1, 1, 0.35f));
        }

        if (Block.TimingRows.Num() > 0 && ToggleGeometry.IsValid())
        {
            const TArray<FVector2f> Toggle =
                Block.bExpanded
                    ? TArray<FVector2f>
                    {
                        FVector2f(ToggleGeometry.Left + 2.0f, ToggleGeometry.Top + 2.0f),
                        FVector2f(ToggleGeometry.Left + 2.0f, ToggleGeometry.Bottom - 2.0f),
                        FVector2f(ToggleGeometry.Right - 1.0f, (ToggleGeometry.Top + ToggleGeometry.Bottom) * 0.5f),
                        FVector2f(ToggleGeometry.Left + 2.0f, ToggleGeometry.Top + 2.0f)
                    }
                    : TArray<FVector2f>
                    {
                        FVector2f(ToggleGeometry.Left + 2.0f, ToggleGeometry.Top + 2.0f),
                        FVector2f(ToggleGeometry.Right - 1.0f, ToggleGeometry.Top + 2.0f),
                        FVector2f((ToggleGeometry.Left + ToggleGeometry.Right) * 0.5f, ToggleGeometry.Bottom - 2.0f),
                        FVector2f(ToggleGeometry.Left + 2.0f, ToggleGeometry.Top + 2.0f)
                    };
            FSlateDrawElement::MakeLines(
                OutDrawElements,
                LayerId + 8,
                AllottedGeometry.ToPaintGeometry(),
                Toggle,
                ESlateDrawEffect::None,
                FLinearColor::White,
                true,
                1.0f);
        }

        DrawTimingRows(OutDrawElements, LayerId + 8, AllottedGeometry, Block, LaneIndex, X0, X1);
    }

    for (const FWeaverKey& Key : Keys)
    {
        const int32 LaneIndex = FindLaneIndex(Key.LaneId);
        if (!Lanes.IsValidIndex(LaneIndex))
        {
            continue;
        }

        double DrawFrame = Key.Frame;
        if (DragMode == EDragMode::Key && DragItemId == Key.KeyId)
        {
            DrawFrame = DragPreviewKeyFrame;
        }

        const FVector2D Center(
            FrameToLocalX(AllottedGeometry, DrawFrame),
            LaneTop(LaneIndex) + LaneHeight * 0.5f);
        const bool bSelected = Selection.Type == EWeaverItemType::Key && Selection.ItemId == Key.KeyId;
        const bool bHovered = HoverSelection.Type == EWeaverItemType::Key && HoverSelection.ItemId == Key.KeyId;
        DrawDiamond(
            OutDrawElements,
            LayerId + 8,
            AllottedGeometry,
            Center,
            Key.Color,
            bSelected,
            bHovered,
            Key.bEnabled);
    }

    const float PlayheadX = FrameToLocalX(AllottedGeometry, CurrentFrame.Get());
    const TArray<FVector2f> Playhead =
    {
        FVector2f(PlayheadX, 0.0f),
        FVector2f(PlayheadX, Size.Y)
    };
    FSlateDrawElement::MakeLines(
        OutDrawElements,
        LayerId + 9,
        AllottedGeometry.ToPaintGeometry(),
        Playhead,
        ESlateDrawEffect::None,
        FLinearColor(0.95f, 0.78f, 0.22f, 0.95f),
        true,
        1.0f);

    return LayerId + 9;
}

SWeaverTimeline::FHitResult SWeaverTimeline::HitTest(
    const FGeometry& Geometry,
    const FVector2D& Local) const
{
    FHitResult Result;
    if (Local.Y < RulerHeight)
    {
        return Result;
    }

    int32 LaneIndex = INDEX_NONE;
    for (int32 Index = 0; Index < Lanes.Num(); ++Index)
    {
        const float Top = LaneTop(Index);
        if (Local.Y >= Top && Local.Y < Top + LaneHeightForIndex(Index))
        {
            LaneIndex = Index;
            break;
        }
    }
    if (!Lanes.IsValidIndex(LaneIndex))
    {
        return Result;
    }

    Result.LaneId = Lanes[LaneIndex].LaneId;

    if (Local.X < TrackLeft())
    {
        const FWeaverLane& Lane = Lanes[LaneIndex];
        const float ActionWidth = 18.0f;
        const float StartX = FMath::Max(4.0f, LeftPadding - ActionWidth * Lane.HeaderActions.Num() - 4.0f);
        if (Local.Y >= LaneTop(LaneIndex) + 7.0f && Local.Y <= LaneTop(LaneIndex) + 25.0f)
        {
            for (int32 ActionIndex = 0; ActionIndex < Lane.HeaderActions.Num(); ++ActionIndex)
            {
                const float X = StartX + ActionIndex * ActionWidth;
                if (Local.X >= X && Local.X <= X + ActionWidth - 2.0f)
                {
                    Result.HeaderActionId = Lane.HeaderActions[ActionIndex].ActionId;
                    Result.bHeaderAction = true;
                    return Result;
                }
            }
        }
        return Result;
    }

    Result.bTrackArea = true;

    const float MainLaneTop = LaneTop(LaneIndex);
    const float MainLaneBottom = MainLaneTop + LaneHeight;
    const float KeyCenterY = MainLaneTop + LaneHeight * 0.5f;
    const float KeyHitRadius = FMath::Min(KeyRadius + 5.0f, LaneHeight * 0.5f);

    for (int32 Index = Keys.Num() - 1; Index >= 0; --Index)
    {
        const FWeaverKey& Key = Keys[Index];
        if (!Key.bEnabled || Key.LaneId != Result.LaneId)
        {
            continue;
        }

        const double DrawFrame = (DragMode == EDragMode::Key && DragItemId == Key.KeyId)
            ? DragPreviewKeyFrame
            : Key.Frame;
        const bool bInMainLane = Local.Y >= MainLaneTop && Local.Y <= MainLaneBottom;
        const bool bNearKeyCenter = FMath::Abs(Local.Y - KeyCenterY) <= KeyHitRadius;
        if (bInMainLane
            && bNearKeyCenter
            && FMath::Abs(FrameToLocalX(Geometry, DrawFrame) - Local.X) <= KeyHitRadius)
        {
            Result.KeyId = Key.KeyId;
            return Result;
        }
    }

    for (int32 Index = Blocks.Num() - 1; Index >= 0; --Index)
    {
        const FWeaverBlock& Block = Blocks[Index];
        if (!Block.bEnabled || Block.LaneId != Result.LaneId)
        {
            continue;
        }

        double DrawStart = Block.StartFrame;
        double DrawEnd = Block.EndFrame;
        if (DragMode == EDragMode::Block && DragItemId == Block.BlockId)
        {
            DrawStart = DragPreviewBlockStart;
            DrawEnd = DragPreviewBlockEnd;
        }

        const float X0 = FrameToLocalX(Geometry, FMath::Min(DrawStart, DrawEnd));
        const float X1 = FrameToLocalX(Geometry, FMath::Max(DrawStart, DrawEnd));
        const float BlockTop = LaneTop(LaneIndex) + BlockVerticalPadding;
        const float BlockHeight = FMath::Max(1.0f, LaneHeight - BlockVerticalPadding * 2.0f);
        const bool bInBlockBody = Local.Y >= BlockTop && Local.Y <= BlockTop + BlockHeight;
        if (bInBlockBody && (Local.X < X0 || Local.X > X1))
        {
            continue;
        }

        if (bInBlockBody)
        {
            Result.BlockId = Block.BlockId;
            Result.BlockEditKind = EWeaverBlockEditKind::Move;
            const FExpansionToggleGeometry ToggleGeometry = GetExpansionToggleGeometry(X0, X1, BlockTop, BlockHeight);
            if (Block.TimingRows.Num() > 0
                && ToggleGeometry.IsValid()
                && Local.X >= ToggleGeometry.Left
                && Local.X <= ToggleGeometry.Right
                && Local.Y >= ToggleGeometry.Top
                && Local.Y <= ToggleGeometry.Bottom)
            {
                Result.bExpansionToggle = true;
                return Result;
            }

            if (Block.bResizable)
            {
                const float StartDistance = FMath::Abs(Local.X - X0);
                const float EndDistance = FMath::Abs(Local.X - X1);
                if (StartDistance <= BlockResizeHandleWidth || EndDistance <= BlockResizeHandleWidth)
                {
                    Result.BlockEditKind = StartDistance <= EndDistance
                        ? EWeaverBlockEditKind::ResizeStart
                        : EWeaverBlockEditKind::ResizeEnd;
                    Result.bBlockEndpoint = true;
                }
            }
            return Result;
        }

        if (Block.bExpanded && Local.X >= X0 && Local.X <= X1)
        {
            for (int32 RowIndex = 0; RowIndex < Block.TimingRows.Num(); ++RowIndex)
            {
                const FWeaverTimingRow& Row = Block.TimingRows[RowIndex];
                const float RowTop = TimingRowTop(LaneIndex, RowIndex);
                if (!Row.bEnabled || Local.Y < RowTop || Local.Y > RowTop + TimingRowHeight)
                {
                    continue;
                }

                const float Width = FMath::Max(1.0f, X1 - X0);
                const float StartX = X0 + Width * FMath::Clamp(Row.StartRatio, 0.0f, 1.0f);
                const float EndX = X0 + Width * FMath::Clamp(Row.EndRatio, Row.StartRatio, 1.0f);
                if (FMath::Abs(Local.X - StartX) <= TimingRowHandleWidth || FMath::Abs(Local.X - EndX) <= TimingRowHandleWidth)
                {
                    Result.BlockId = Block.BlockId;
                    Result.TimingRowId = Row.RowId;
                    Result.bTimingRow = true;
                    Result.bTimingStartHandle = FMath::Abs(Local.X - StartX) <= FMath::Abs(Local.X - EndX);
                    return Result;
                }
            }
        }
    }

    return Result;
}

void SWeaverTimeline::ApplySelection(const FWeaverSelection& NewSelection, const bool bNotify)
{
    if (Selection == NewSelection)
    {
        return;
    }

    Selection = NewSelection;
    Invalidate(EInvalidateWidgetReason::Paint);
    if (bNotify)
    {
        OnSelectionChanged.ExecuteIfBound(Selection);
    }
}

void SWeaverTimeline::RequestViewRange(const double NewStart, const double NewEnd)
{
    if (!FMath::IsFinite(NewStart) || !FMath::IsFinite(NewEnd) || NewEnd <= NewStart)
    {
        return;
    }

    if (!bExternalViewRange)
    {
        ViewStartFrame = NewStart;
        ViewEndFrame = NewEnd;
        Invalidate(EInvalidateWidgetReason::Paint);
    }

    OnViewRangeChanged.ExecuteIfBound(NewStart, NewEnd);
}

void SWeaverTimeline::UpdateHover(const FGeometry& Geometry, const FVector2D& Local)
{
    const FWeaverSelection NewHover = HitTest(Geometry, Local).ToSelection();
    if (HoverSelection != NewHover)
    {
        HoverSelection = NewHover;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

void SWeaverTimeline::BeginKeyDrag(const FWeaverKey& Key, const FVector2D& Local)
{
    DragMode = EDragMode::Key;
    DragStartLocal = Local;
    DragLaneId = Key.LaneId;
    DragItemId = Key.KeyId;
    DragOriginalKeyFrame = Key.Frame;
    DragPreviewKeyFrame = Key.Frame;
    OnKeyEditStarted.ExecuteIfBound(Key.LaneId, Key.KeyId, Key.Frame);
}

void SWeaverTimeline::BeginBlockDrag(
    const FWeaverBlock& Block,
    const EWeaverBlockEditKind Kind,
    const FVector2D& Local)
{
    DragMode = EDragMode::Block;
    DragStartLocal = Local;
    DragLaneId = Block.LaneId;
    DragItemId = Block.BlockId;
    DragBlockEditKind = Kind;
    DragOriginalBlockStart = Block.StartFrame;
    DragOriginalBlockEnd = Block.EndFrame;
    DragPreviewBlockStart = Block.StartFrame;
    DragPreviewBlockEnd = Block.EndFrame;
    OnBlockEditStarted.ExecuteIfBound(Block.LaneId, Block.BlockId, Kind);
}

void SWeaverTimeline::BeginTimingRowDrag(
    const FWeaverBlock& Block,
    const FWeaverTimingRow& Row,
    const bool bStartHandle,
    const FVector2D& Local)
{
    DragMode = EDragMode::TimingRow;
    DragStartLocal = Local;
    DragLaneId = Block.LaneId;
    DragTimingBlockId = Block.BlockId;
    DragTimingRowId = Row.RowId;
    bDragTimingStartHandle = bStartHandle;
    DragOriginalTimingStart = FMath::Clamp(Row.StartRatio, 0.0f, 1.0f);
    DragOriginalTimingEnd = FMath::Clamp(Row.EndRatio, DragOriginalTimingStart, 1.0f);
    DragPreviewTimingStart = DragOriginalTimingStart;
    DragPreviewTimingEnd = DragOriginalTimingEnd;
    OnTimingRowEditStarted.ExecuteIfBound(
        Block.LaneId,
        Block.BlockId,
        Row.RowId,
        DragOriginalTimingStart,
        DragOriginalTimingEnd);
}

void SWeaverTimeline::FinishPendingEndpointClick()
{
    const FGuid Lane = DragLaneId;
    const FGuid Item = DragItemId;
    const auto Kind = PendingEndpointKind;
    DragMode = EDragMode::None;
    DragLaneId.Invalidate();
    DragItemId.Invalidate();
    PendingEndpointKind = EWeaverBlockEditKind::Move;
    Invalidate(EInvalidateWidgetReason::Paint);
    if (Item.IsValid() && Kind != EWeaverBlockEditKind::Move)
    {
        OnBlockEndpointClicked.ExecuteIfBound(Lane, Item, Kind == EWeaverBlockEditKind::ResizeStart);
    }
}

void SWeaverTimeline::ToggleBlockExpansion(const FGuid& BlockId)
{
    if (FWeaverBlock* Block = Blocks.FindByPredicate([&BlockId](FWeaverBlock& Candidate)
        {
            return Candidate.BlockId == BlockId;
        }))
    {
        Block->bExpanded = !Block->bExpanded;
        OnBlockExpansionChanged.ExecuteIfBound(Block->BlockId, Block->bExpanded);
        Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
    }
}

void SWeaverTimeline::FinishPrimaryInteraction(const bool bCancelled)
{
    TFunction<void()> Notify;
    if (DragMode == EDragMode::Key && DragItemId.IsValid())
    {
        const double FinalFrame = bCancelled ? DragOriginalKeyFrame : DragPreviewKeyFrame;
        Notify = [Delegate = OnKeyEditFinished, Lane = DragLaneId, Item = DragItemId, FinalFrame, bCancelled]()
        { Delegate.ExecuteIfBound(Lane, Item, FinalFrame, bCancelled); };
    }
    else if (DragMode == EDragMode::Block && DragItemId.IsValid())
    {
        const double FinalStart = bCancelled ? DragOriginalBlockStart : DragPreviewBlockStart;
        const double FinalEnd = bCancelled ? DragOriginalBlockEnd : DragPreviewBlockEnd;
        Notify = [Delegate = OnBlockEditFinished, Lane = DragLaneId, Item = DragItemId, FinalStart, FinalEnd, bCancelled]()
        { Delegate.ExecuteIfBound(Lane, Item, FinalStart, FinalEnd, bCancelled); };
    }
    else if (DragMode == EDragMode::TimingRow && DragTimingBlockId.IsValid())
    {
        const float FinalStart = bCancelled ? DragOriginalTimingStart : DragPreviewTimingStart;
        const float FinalEnd = bCancelled ? DragOriginalTimingEnd : DragPreviewTimingEnd;
        Notify = [Delegate = OnTimingRowEditFinished, Lane = DragLaneId, Item = DragTimingBlockId,
            Row = DragTimingRowId, FinalStart, FinalEnd, bCancelled]()
        { Delegate.ExecuteIfBound(Lane, Item, Row, FinalStart, FinalEnd, bCancelled); };
    }

    DragMode = EDragMode::None;
    DragLaneId.Invalidate();
    DragItemId.Invalidate();
    DragOriginalKeyFrame = 0.0;
    DragPreviewKeyFrame = 0.0;
    DragBlockEditKind = EWeaverBlockEditKind::Move;
    DragOriginalBlockStart = 0.0;
    DragOriginalBlockEnd = 0.0;
    DragPreviewBlockStart = 0.0;
    DragPreviewBlockEnd = 0.0;
    PendingEndpointKind = EWeaverBlockEditKind::Move;
    DragTimingBlockId.Invalidate();
    DragTimingRowId = NAME_None;
    bDragTimingStartHandle = false;
    DragOriginalTimingStart = 0.0f;
    DragOriginalTimingEnd = 1.0f;
    DragPreviewTimingStart = 0.0f;
    DragPreviewTimingEnd = 1.0f;
    Invalidate(EInvalidateWidgetReason::Paint);
    if (Notify) { Notify(); }
}

void SWeaverTimeline::ResetRightMouseState()
{
    bRightButtonDown = false;
    bRightPressMoved = false;
    RightPressLocal = FVector2D::ZeroVector;
    RightPressScreen = FVector2D::ZeroVector;
    RightPressHit = FHitResult();
}

FReply SWeaverTimeline::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    const FHitResult Hit = HitTest(MyGeometry, Local);

    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (Hit.bHeaderAction)
        {
            if (const FWeaverLane* Lane = Lanes.FindByPredicate([&Hit](const FWeaverLane& Candidate)
                {
                    return Candidate.LaneId == Hit.LaneId;
                }))
            {
                if (const FWeaverLaneHeaderAction* Action = Lane->HeaderActions.FindByPredicate([&Hit](const FWeaverLaneHeaderAction& Candidate)
                    {
                        return Candidate.ActionId == Hit.HeaderActionId;
                    }))
                {
                    if (Action->bEnabled)
                    {
                        OnLaneHeaderActionRequested.ExecuteIfBound(Hit.LaneId, Hit.HeaderActionId);
                    }
                }
            }
            return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
        }

        if (Hit.KeyId.IsValid())
        {
            ApplySelection(Hit.ToSelection(), true);
            if (const FWeaverKey* Key = FindKey(Hit.KeyId))
            {
                BeginKeyDrag(*Key, Local);
                if (DragMode != EDragMode::Key) { return FReply::Handled(); }
                return FReply::Handled()
                    .CaptureMouse(SharedThis(this))
                    .SetUserFocus(SharedThis(this), EFocusCause::Mouse);
            }
        }

        if (Hit.BlockId.IsValid())
        {
            ApplySelection(Hit.ToSelection(), true);
            if (const FWeaverBlock* Block = FindBlock(Hit.BlockId))
            {
                if (Hit.bExpansionToggle)
                {
                    ToggleBlockExpansion(Block->BlockId);
                    return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
                }

                if (Hit.bTimingRow)
                {
                    if (const FWeaverTimingRow* Row = Block->TimingRows.FindByPredicate([&Hit](const FWeaverTimingRow& Candidate)
                        {
                            return Candidate.RowId == Hit.TimingRowId;
                        }))
                    {
                        BeginTimingRowDrag(*Block, *Row, Hit.bTimingStartHandle, Local);
                        if (DragMode != EDragMode::TimingRow) { return FReply::Handled(); }
                        return FReply::Handled()
                            .CaptureMouse(SharedThis(this))
                            .SetUserFocus(SharedThis(this), EFocusCause::Mouse);
                    }
                }

                if (Hit.bBlockEndpoint)
                {
                    DragMode = EDragMode::PendingBlockEndpoint;
                    DragStartLocal = Local;
                    DragLaneId = Block->LaneId;
                    DragItemId = Block->BlockId;
                    PendingEndpointKind = Hit.BlockEditKind;
                    return FReply::Handled()
                        .CaptureMouse(SharedThis(this))
                        .SetUserFocus(SharedThis(this), EFocusCause::Mouse);
                }

                BeginBlockDrag(*Block, Hit.BlockEditKind, Local);
                if (DragMode != EDragMode::Block) { return FReply::Handled(); }
                return FReply::Handled()
                    .CaptureMouse(SharedThis(this))
                    .SetUserFocus(SharedThis(this), EFocusCause::Mouse);
            }
        }

        if (Hit.bTrackArea)
        {
            FWeaverSelection Empty;
            ApplySelection(Empty, true);
            DragMode = EDragMode::Scrub;
            DragStartLocal = Local;
            OnFrameChanged.ExecuteIfBound(LocalXToFrame(MyGeometry, Local.X, true));
            return FReply::Handled()
                .CaptureMouse(SharedThis(this))
                .SetUserFocus(SharedThis(this), EFocusCause::Mouse);
        }
    }

    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && Hit.bTrackArea)
    {
        if (Hit.KeyId.IsValid() || Hit.BlockId.IsValid())
        {
            ApplySelection(Hit.ToSelection(), true);
        }

        bRightButtonDown = true;
        bRightPressMoved = false;
        RightPressLocal = Local;
        RightPressScreen = MouseEvent.GetScreenSpacePosition();
        RightPressHit = Hit;
        PanStartViewStart = ViewStartFrame;
        PanStartViewEnd = ViewEndFrame;
        return FReply::Handled()
            .CaptureMouse(SharedThis(this))
            .SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }

    return FReply::Unhandled();
}

FReply SWeaverTimeline::OnMouseButtonUp(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bRightButtonDown)
    {
        if (!bRightPressMoved)
        {
            const FWeaverSelection ContextSelection = RightPressHit.ToSelection();
            if (ContextSelection.IsValid())
            {
                OnContextRequested.ExecuteIfBound(
                    ContextSelection.Type,
                    ContextSelection.LaneId,
                    ContextSelection.ItemId,
                    RightPressScreen);
            }
            else if (RightPressHit.bTrackArea && RightPressHit.LaneId.IsValid())
            {
                OnLaneContextRequested.ExecuteIfBound(
                    RightPressHit.LaneId,
                    LocalXToFrame(MyGeometry, RightPressLocal.X, true),
                    RightPressScreen);
            }
        }

        ResetRightMouseState();
        return FReply::Handled().ReleaseMouseCapture();
    }

    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && DragMode != EDragMode::None)
    {
        if (DragMode == EDragMode::PendingBlockEndpoint)
        {
            FinishPendingEndpointClick();
        }
        else
        {
            FinishPrimaryInteraction(false);
        }
        return FReply::Handled().ReleaseMouseCapture();
    }

    return FReply::Unhandled();
}

FReply SWeaverTimeline::OnMouseMove(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    if (bRightButtonDown && HasMouseCapture())
    {
        if (!bRightPressMoved && FVector2D::Distance(Local, RightPressLocal) >= RightPanThreshold)
        {
            bRightPressMoved = true;
        }

        if (bRightPressMoved)
        {
            const double DeltaFrames = -PixelsToFrames(MyGeometry, Local.X - RightPressLocal.X);
            RequestViewRange(PanStartViewStart + DeltaFrames, PanStartViewEnd + DeltaFrames);
        }
        return FReply::Handled();
    }

    if (!HasMouseCapture())
    {
        UpdateHover(MyGeometry, Local);
        return FReply::Unhandled();
    }

    if (DragMode == EDragMode::PendingBlockEndpoint)
    {
        if (FVector2D::Distance(Local, DragStartLocal) >= PrimaryDragThreshold)
        {
            if (const FWeaverBlock* Block = FindBlock(DragItemId))
            {
                BeginBlockDrag(*Block, PendingEndpointKind, DragStartLocal);
            }
        }
        else
        {
            return FReply::Handled();
        }
    }

    if (DragMode == EDragMode::Scrub)
    {
        OnFrameChanged.ExecuteIfBound(LocalXToFrame(MyGeometry, Local.X, true));
        return FReply::Handled();
    }

    const double DeltaFrames = PixelsToFrames(MyGeometry, Local.X - DragStartLocal.X);
    if (DragMode == EDragMode::Key && DragItemId.IsValid())
    {
        DragPreviewKeyFrame = DragOriginalKeyFrame + DeltaFrames;
        OnKeyEditChanged.ExecuteIfBound(DragLaneId, DragItemId, DragPreviewKeyFrame);
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    if (DragMode == EDragMode::Block && DragItemId.IsValid())
    {
        switch (DragBlockEditKind)
        {
        case EWeaverBlockEditKind::Move:
            DragPreviewBlockStart = DragOriginalBlockStart + DeltaFrames;
            DragPreviewBlockEnd = DragOriginalBlockEnd + DeltaFrames;
            break;

        case EWeaverBlockEditKind::ResizeStart:
            DragPreviewBlockStart = FMath::Min(
                DragOriginalBlockStart + DeltaFrames,
                DragOriginalBlockEnd - MinBlockDurationFrames);
            DragPreviewBlockEnd = DragOriginalBlockEnd;
            break;

        case EWeaverBlockEditKind::ResizeEnd:
            DragPreviewBlockStart = DragOriginalBlockStart;
            DragPreviewBlockEnd = FMath::Max(
                DragOriginalBlockEnd + DeltaFrames,
                DragOriginalBlockStart + MinBlockDurationFrames);
            break;
        }

        OnBlockEditChanged.ExecuteIfBound(
            DragLaneId,
            DragItemId,
            DragBlockEditKind,
            DragPreviewBlockStart,
            DragPreviewBlockEnd);
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    if (DragMode == EDragMode::TimingRow && DragTimingBlockId.IsValid())
    {
        const FWeaverBlock* Block = FindBlock(DragTimingBlockId);
        if (Block)
        {
            const double DrawStart = Block->StartFrame;
            const double DrawEnd = Block->EndFrame;
            const float X0 = FrameToLocalX(MyGeometry, FMath::Min(DrawStart, DrawEnd));
            const float X1 = FrameToLocalX(MyGeometry, FMath::Max(DrawStart, DrawEnd));
            const float Width = FMath::Max(1.0f, X1 - X0);
            const float Ratio = FMath::Clamp((Local.X - X0) / Width, 0.0f, 1.0f);
            if (bDragTimingStartHandle)
            {
                DragPreviewTimingStart = FMath::Min(Ratio, DragPreviewTimingEnd - 0.001f);
                DragPreviewTimingStart = FMath::Clamp(DragPreviewTimingStart, 0.0f, 1.0f);
            }
            else
            {
                DragPreviewTimingEnd = FMath::Max(Ratio, DragPreviewTimingStart + 0.001f);
                DragPreviewTimingEnd = FMath::Clamp(DragPreviewTimingEnd, 0.0f, 1.0f);
            }

            OnTimingRowEditChanged.ExecuteIfBound(
                Block->LaneId,
                DragTimingBlockId,
                DragTimingRowId,
                DragPreviewTimingStart,
                DragPreviewTimingEnd);
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled();
        }
    }

    return FReply::Unhandled();
}

FReply SWeaverTimeline::OnMouseWheel(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (FMath::IsNearlyZero(MouseEvent.GetWheelDelta()))
    {
        return FReply::Unhandled();
    }

    const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    const double Anchor = LocalXToFrame(MyGeometry, Local.X, true);
    const double Scale = FMath::Pow(0.82, static_cast<double>(MouseEvent.GetWheelDelta()));
    const double NewStart = Anchor + (ViewStartFrame - Anchor) * Scale;
    const double NewEnd = Anchor + (ViewEndFrame - Anchor) * Scale;
    if (NewEnd - NewStart < 1.0)
    {
        return FReply::Handled();
    }

    RequestViewRange(NewStart, NewEnd);
    return FReply::Handled();
}

FReply SWeaverTimeline::OnKeyDown(
    const FGeometry& MyGeometry,
    const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        const bool bHadInteraction = DragMode != EDragMode::None || bRightButtonDown;
        if (DragMode != EDragMode::None)
        {
            FinishPrimaryInteraction(true);
        }
        ResetRightMouseState();
        if (bHadInteraction)
        {
            return FReply::Handled().ReleaseMouseCapture();
        }
    }

    if (InKeyEvent.GetKey() != EKeys::Delete && InKeyEvent.GetKey() != EKeys::BackSpace)
    {
        return FReply::Unhandled();
    }

    if (!Selection.IsValid())
    {
        return FReply::Unhandled();
    }

    const FWeaverSelection Deleted = Selection;
    if (!bDeferDeleteSelectionToSource) { ClearSelection(true); }
    OnDeleteRequested.ExecuteIfBound(Deleted.Type, Deleted.LaneId, Deleted.ItemId);
    return FReply::Handled();
}

void SWeaverTimeline::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    if (DragMode != EDragMode::None)
    {
        FinishPrimaryInteraction(true);
    }
    ResetRightMouseState();
    SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
}

void SWeaverTimeline::OnMouseLeave(const FPointerEvent& MouseEvent)
{
    if (!HasMouseCapture() && HoverSelection.IsValid())
    {
        HoverSelection.Reset();
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    SLeafWidget::OnMouseLeave(MouseEvent);
}

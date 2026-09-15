#include "SWeaverTimeline.h"

#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

namespace
{
constexpr float KeyRadius = 5.0f;
constexpr float BlockVerticalPadding = 4.0f;
constexpr float LabelTextPadding = 10.0f;
constexpr float BlockResizeHandleWidth = 7.0f;
constexpr float RightPanThreshold = 4.0f;
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
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SWeaverTimeline::SetSelection(const FWeaverSelection& InSelection)
{
    ApplySelection(InSelection, false);
}

void SWeaverTimeline::ClearSelection()
{
    FWeaverSelection Empty;
    ApplySelection(Empty, false);
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
    return FVector2D(900.0f, RulerHeight + FMath::Max(1, Lanes.Num()) * LaneHeight);
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
    return RulerHeight + LaneIndex * LaneHeight;
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
        if (LaneIndex % 2 == 1)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 1,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f(Size.X, LaneHeight),
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

        if (!Block.Label.IsEmpty() && Width > 28.0f)
        {
            FSlateDrawElement::MakeText(
                OutDrawElements,
                LayerId + 7,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f(Width - 8.0f, Height - 4.0f),
                    FSlateLayoutTransform(FVector2f(X0 + 6.0f, Top + 4.0f))),
                Block.Label,
                FAppStyle::GetFontStyle("SmallFont"),
                ESlateDrawEffect::None,
                Block.bEnabled ? FLinearColor::White : FLinearColor(1, 1, 1, 0.35f));
        }
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
    if (Local.X < TrackLeft() || Local.Y < RulerHeight)
    {
        return Result;
    }

    const int32 LaneIndex = FMath::FloorToInt((Local.Y - RulerHeight) / LaneHeight);
    if (!Lanes.IsValidIndex(LaneIndex))
    {
        return Result;
    }

    Result.LaneId = Lanes[LaneIndex].LaneId;
    Result.bTrackArea = true;

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
        if (FMath::Abs(FrameToLocalX(Geometry, DrawFrame) - Local.X) <= KeyRadius + 5.0f)
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
        if (Local.X < X0 || Local.X > X1)
        {
            continue;
        }

        Result.BlockId = Block.BlockId;
        Result.BlockEditKind = EWeaverBlockEditKind::Move;
        if (Block.bResizable)
        {
            if (FMath::Abs(Local.X - X0) <= BlockResizeHandleWidth)
            {
                Result.BlockEditKind = EWeaverBlockEditKind::ResizeStart;
            }
            else if (FMath::Abs(Local.X - X1) <= BlockResizeHandleWidth)
            {
                Result.BlockEditKind = EWeaverBlockEditKind::ResizeEnd;
            }
        }
        return Result;
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

void SWeaverTimeline::FinishPrimaryInteraction(const bool bCancelled)
{
    if (DragMode == EDragMode::Key && DragItemId.IsValid())
    {
        const double FinalFrame = bCancelled ? DragOriginalKeyFrame : DragPreviewKeyFrame;
        OnKeyEditFinished.ExecuteIfBound(DragLaneId, DragItemId, FinalFrame, bCancelled);
    }
    else if (DragMode == EDragMode::Block && DragItemId.IsValid())
    {
        const double FinalStart = bCancelled ? DragOriginalBlockStart : DragPreviewBlockStart;
        const double FinalEnd = bCancelled ? DragOriginalBlockEnd : DragPreviewBlockEnd;
        OnBlockEditFinished.ExecuteIfBound(DragLaneId, DragItemId, FinalStart, FinalEnd, bCancelled);
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
    Invalidate(EInvalidateWidgetReason::Paint);
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
        if (Hit.KeyId.IsValid())
        {
            ApplySelection(Hit.ToSelection(), true);
            if (const FWeaverKey* Key = FindKey(Hit.KeyId))
            {
                BeginKeyDrag(*Key, Local);
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
                BeginBlockDrag(*Block, Hit.BlockEditKind, Local);
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
        }

        ResetRightMouseState();
        return FReply::Handled().ReleaseMouseCapture();
    }

    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && DragMode != EDragMode::None)
    {
        FinishPrimaryInteraction(false);
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
    FWeaverSelection Empty;
    ApplySelection(Empty, true);
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

#pragma once

#include "CoreMinimal.h"
#include "WeaverTimelineTypes.h"
#include "Widgets/SLeafWidget.h"

DECLARE_DELEGATE_OneParam(FOnWeaverFrameChanged, double)
DECLARE_DELEGATE_OneParam(FOnWeaverSelectionChanged, FWeaverSelection)
DECLARE_DELEGATE_ThreeParams(FOnWeaverKeyEditStarted, FGuid, FGuid, double)
DECLARE_DELEGATE_ThreeParams(FOnWeaverKeyEditChanged, FGuid, FGuid, double)
DECLARE_DELEGATE_FourParams(FOnWeaverKeyEditFinished, FGuid, FGuid, double, bool)
DECLARE_DELEGATE_ThreeParams(FOnWeaverBlockEditStarted, FGuid, FGuid, EWeaverBlockEditKind)
DECLARE_DELEGATE_FiveParams(FOnWeaverBlockEditChanged, FGuid, FGuid, EWeaverBlockEditKind, double, double)
DECLARE_DELEGATE_FiveParams(FOnWeaverBlockEditFinished, FGuid, FGuid, double, double, bool)
DECLARE_DELEGATE_ThreeParams(FOnWeaverDeleteRequested, EWeaverItemType, FGuid, FGuid)
DECLARE_DELEGATE_FourParams(FOnWeaverContextRequested, EWeaverItemType, FGuid, FGuid, FVector2D)
DECLARE_DELEGATE_TwoParams(FOnWeaverViewRangeChanged, double, double)
DECLARE_DELEGATE_TwoParams(FOnWeaverLaneHeaderActionRequested, FGuid, FName)
DECLARE_DELEGATE_ThreeParams(FOnWeaverBlockEndpointClicked, FGuid, FGuid, bool)
DECLARE_DELEGATE_ThreeParams(FOnWeaverLaneContextRequested, FGuid, double, FVector2D)
DECLARE_DELEGATE_TwoParams(FOnWeaverBlockExpansionChanged, FGuid, bool)
DECLARE_DELEGATE_FiveParams(FOnWeaverTimingRowEditStarted, FGuid, FGuid, FName, float, float)
DECLARE_DELEGATE_FiveParams(FOnWeaverTimingRowEditChanged, FGuid, FGuid, FName, float, float)
DECLARE_DELEGATE_SixParams(FOnWeaverTimingRowEditFinished, FGuid, FGuid, FName, float, float, bool)

class SWeaverTimeline final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SWeaverTimeline)
        : _CurrentFrame(0.0)
        , _RulerHeight(24.0f)
        , _LaneHeight(36.0f)
        , _LabelWidth(148.0f)
        , _DeferDeleteSelectionToSource(false)
    {}
        SLATE_ATTRIBUTE(double, CurrentFrame)
        SLATE_ARGUMENT(float, RulerHeight)
        SLATE_ARGUMENT(float, LaneHeight)
        SLATE_ARGUMENT(float, LabelWidth)
        SLATE_ARGUMENT(bool, DeferDeleteSelectionToSource)
        SLATE_EVENT(FOnWeaverFrameChanged, OnFrameChanged)
        SLATE_EVENT(FOnWeaverSelectionChanged, OnSelectionChanged)
        SLATE_EVENT(FOnWeaverKeyEditStarted, OnKeyEditStarted)
        SLATE_EVENT(FOnWeaverKeyEditChanged, OnKeyEditChanged)
        SLATE_EVENT(FOnWeaverKeyEditFinished, OnKeyEditFinished)
        SLATE_EVENT(FOnWeaverBlockEditStarted, OnBlockEditStarted)
        SLATE_EVENT(FOnWeaverBlockEditChanged, OnBlockEditChanged)
        SLATE_EVENT(FOnWeaverBlockEditFinished, OnBlockEditFinished)
        SLATE_EVENT(FOnWeaverDeleteRequested, OnDeleteRequested)
        SLATE_EVENT(FOnWeaverContextRequested, OnContextRequested)
        SLATE_EVENT(FOnWeaverViewRangeChanged, OnViewRangeChanged)
        SLATE_EVENT(FOnWeaverLaneHeaderActionRequested, OnLaneHeaderActionRequested)
        SLATE_EVENT(FOnWeaverBlockEndpointClicked, OnBlockEndpointClicked)
        SLATE_EVENT(FOnWeaverLaneContextRequested, OnLaneContextRequested)
        SLATE_EVENT(FOnWeaverBlockExpansionChanged, OnBlockExpansionChanged)
        SLATE_EVENT(FOnWeaverTimingRowEditStarted, OnTimingRowEditStarted)
        SLATE_EVENT(FOnWeaverTimingRowEditChanged, OnTimingRowEditChanged)
        SLATE_EVENT(FOnWeaverTimingRowEditFinished, OnTimingRowEditFinished)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    void SetLanes(TArray<FWeaverLane> InLanes);
    void SetKeys(TArray<FWeaverKey> InKeys);
    void SetBlocks(TArray<FWeaverBlock> InBlocks);
    /** Atomic authority publication. Retains selection only while its stable ID still exists. */
    void SetPresentation(TArray<FWeaverLane> InLanes, TArray<FWeaverKey> InKeys, TArray<FWeaverBlock> InBlocks);
    void CancelInteraction();
    void UnbindCallbacks();
    const TArray<FWeaverBlock>& GetDisplayedBlocks() const { return Blocks; }
    const TArray<FWeaverKey>& GetDisplayedKeys() const { return Keys; }

    void SetSelection(const FWeaverSelection& InSelection);
    void ClearSelection(bool bNotify = false);
    const FWeaverSelection& GetSelection() const { return Selection; }

    void SetExternalViewRange(double StartFrame, double EndFrame);
    void ClearExternalViewRange();
    void SetHorizontalPadding(float Left, float Right);

    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
    virtual int32 OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
    virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
    virtual bool SupportsKeyboardFocus() const override { return true; }

private:
    enum class EDragMode : uint8
    {
        None,
        Scrub,
        Key,
        Block,
        PendingBlockEndpoint,
        TimingRow
    };

    struct FHitResult
    {
        FGuid LaneId;
        FGuid KeyId;
        FGuid BlockId;
        FName HeaderActionId;
        FName TimingRowId;
        EWeaverBlockEditKind BlockEditKind = EWeaverBlockEditKind::Move;
        bool bHeaderAction = false;
        bool bExpansionToggle = false;
        bool bTimingRow = false;
        bool bTimingStartHandle = false;
        bool bBlockEndpoint = false;
        bool bTrackArea = false;

        FWeaverSelection ToSelection() const;
    };

    struct FExpansionToggleGeometry
    {
        float Left = 0.0f;
        float Top = 0.0f;
        float Right = 0.0f;
        float Bottom = 0.0f;

        bool IsValid() const
        {
            return Right > Left && Bottom > Top;
        }
    };

    int32 FindLaneIndex(const FGuid& LaneId) const;
    const FWeaverKey* FindKey(const FGuid& KeyId) const;
    const FWeaverBlock* FindBlock(const FGuid& BlockId) const;

    float TrackLeft() const;
    float TrackRight(const FGeometry& Geometry) const;
    float LaneTop(int32 LaneIndex) const;
    float LaneHeightForIndex(int32 LaneIndex) const;
    int32 ExpandedTimingRowCount(int32 LaneIndex) const;
    float TimingRowTop(int32 LaneIndex, int32 RowIndex) const;
    FExpansionToggleGeometry GetExpansionToggleGeometry(float BlockX0, float BlockX1, float BlockTop, float BlockHeight) const;
    float FrameToLocalX(const FGeometry& Geometry, double Frame) const;
    double LocalXToFrame(const FGeometry& Geometry, float X, bool bClampToView) const;
    double PixelsToFrames(const FGeometry& Geometry, float DeltaX) const;

    FHitResult HitTest(const FGeometry& Geometry, const FVector2D& Local) const;
    void DrawRuler(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry) const;
    void DrawLaneHeaderActions(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, int32 LaneIndex) const;
    void DrawTimingRows(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FWeaverBlock& Block, int32 LaneIndex, float BlockX0, float BlockX1) const;
    void DrawDiamond(
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FGeometry& Geometry,
        const FVector2D& Center,
        const FLinearColor& Color,
        bool bSelected,
        bool bHovered,
        bool bEnabled) const;
    double ChooseTickStep(double VisibleFrames, float TrackWidth) const;

    void ApplySelection(const FWeaverSelection& NewSelection, bool bNotify);
    void RequestViewRange(double NewStart, double NewEnd);
    void UpdateHover(const FGeometry& Geometry, const FVector2D& Local);
    void BeginKeyDrag(const FWeaverKey& Key, const FVector2D& Local);
    void BeginBlockDrag(const FWeaverBlock& Block, EWeaverBlockEditKind Kind, const FVector2D& Local);
    void BeginTimingRowDrag(const FWeaverBlock& Block, const FWeaverTimingRow& Row, bool bStartHandle, const FVector2D& Local);
    void FinishPendingEndpointClick();
    void ToggleBlockExpansion(const FGuid& BlockId);
    void FinishPrimaryInteraction(bool bCancelled);
    void ResetRightMouseState();

    TAttribute<double> CurrentFrame;
    FOnWeaverFrameChanged OnFrameChanged;
    FOnWeaverSelectionChanged OnSelectionChanged;
    FOnWeaverKeyEditStarted OnKeyEditStarted;
    FOnWeaverKeyEditChanged OnKeyEditChanged;
    FOnWeaverKeyEditFinished OnKeyEditFinished;
    FOnWeaverBlockEditStarted OnBlockEditStarted;
    FOnWeaverBlockEditChanged OnBlockEditChanged;
    FOnWeaverBlockEditFinished OnBlockEditFinished;
    FOnWeaverDeleteRequested OnDeleteRequested;
    FOnWeaverContextRequested OnContextRequested;
    FOnWeaverViewRangeChanged OnViewRangeChanged;
    FOnWeaverLaneHeaderActionRequested OnLaneHeaderActionRequested;
    FOnWeaverBlockEndpointClicked OnBlockEndpointClicked;
    FOnWeaverLaneContextRequested OnLaneContextRequested;
    FOnWeaverBlockExpansionChanged OnBlockExpansionChanged;
    FOnWeaverTimingRowEditStarted OnTimingRowEditStarted;
    FOnWeaverTimingRowEditChanged OnTimingRowEditChanged;
    FOnWeaverTimingRowEditFinished OnTimingRowEditFinished;

    TArray<FWeaverLane> Lanes;
    TArray<FWeaverKey> Keys;
    TArray<FWeaverBlock> Blocks;

    FWeaverSelection Selection;
    FWeaverSelection HoverSelection;

    double ViewStartFrame = 0.0;
    double ViewEndFrame = 240.0;
    bool bExternalViewRange = false;
    bool bDeferDeleteSelectionToSource = false;
    float RulerHeight = 24.0f;
    float LaneHeight = 36.0f;
    float LabelWidth = 148.0f;
    float LeftPadding = 148.0f;
    float RightPadding = 12.0f;

    EDragMode DragMode = EDragMode::None;
    FVector2D DragStartLocal = FVector2D::ZeroVector;
    FGuid DragLaneId;
    FGuid DragItemId;

    double DragOriginalKeyFrame = 0.0;
    double DragPreviewKeyFrame = 0.0;

    EWeaverBlockEditKind DragBlockEditKind = EWeaverBlockEditKind::Move;
    double DragOriginalBlockStart = 0.0;
    double DragOriginalBlockEnd = 0.0;
    double DragPreviewBlockStart = 0.0;
    double DragPreviewBlockEnd = 0.0;

    EWeaverBlockEditKind PendingEndpointKind = EWeaverBlockEditKind::Move;
    FGuid DragTimingBlockId;
    FName DragTimingRowId;
    bool bDragTimingStartHandle = false;
    float DragOriginalTimingStart = 0.0f;
    float DragOriginalTimingEnd = 1.0f;
    float DragPreviewTimingStart = 0.0f;
    float DragPreviewTimingEnd = 1.0f;

    bool bRightButtonDown = false;
    bool bRightPressMoved = false;
    FVector2D RightPressLocal = FVector2D::ZeroVector;
    FVector2D RightPressScreen = FVector2D::ZeroVector;
    FHitResult RightPressHit;
    double PanStartViewStart = 0.0;
    double PanStartViewEnd = 240.0;
};

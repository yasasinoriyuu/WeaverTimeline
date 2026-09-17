#include "WeaverViewportOverlay.h"

#include "IAssetViewport.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "SWeaverTimelineHost.h"
#include "SWeaverEditableTimeline.h"
#include "Widgets/SOverlay.h"

FWeaverViewportOverlay::~FWeaverViewportOverlay()
{
    Unregister();
    bVisible = true;
}

void FWeaverViewportOverlay::Register(
    const TSharedRef<SWidget>& InContent,
    const FText& InCollapsedLabel,
    const float InExpandedHeight,
    FSimpleDelegate InOnDeactivated)
{
    Unregister();
    OnDeactivated = InOnDeactivated;

    OverlayWidget =
        SNew(SOverlay)
        .Visibility(EVisibility::SelfHitTestInvisible)
        + SOverlay::Slot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Bottom)
        .Padding(FMargin(8.0f))
        [
            SNew(SWeaverTimelineHost)
            .ExpandedHeight(InExpandedHeight)
            .CollapsedLabel(InCollapsedLabel)
            .OnDeactivated(OnDeactivated)
            [
                InContent
            ]
        ];

    SetVisible(bVisible);

    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FWeaverViewportOverlay::Tick),
        1.0f);

    Tick(0.0f);
}

void FWeaverViewportOverlay::Unregister()
{
    OnDeactivated.ExecuteIfBound();
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }

    DetachFromViewport();
    OverlayWidget.Reset();
    OnDeactivated.Unbind();
}

void FWeaverViewportOverlay::RegisterEditable(const TSharedRef<SWeaverEditableTimeline>& InContent,
    const FText& InCollapsedLabel, float InExpandedHeight)
{
    Register(InContent, InCollapsedLabel, InExpandedHeight,
        FSimpleDelegate::CreateSP(InContent, &SWeaverEditableTimeline::Deactivate));
}

void FWeaverViewportOverlay::SetVisible(const bool bInVisible)
{
    if (!bInVisible) { OnDeactivated.ExecuteIfBound(); }
    bVisible = bInVisible;
    if (OverlayWidget.IsValid())
    {
        OverlayWidget->SetVisibility(bVisible ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
    }
}

bool FWeaverViewportOverlay::Tick(float DeltaTime)
{
    AttachToActiveViewport();
    return true;
}

void FWeaverViewportOverlay::AttachToActiveViewport()
{
    FLevelEditorModule* LevelEditorModule =
        FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");

    const TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule
        ? LevelEditorModule->GetFirstActiveViewport()
        : nullptr;

    if (ActiveViewport == AttachedViewport.Pin())
    {
        return;
    }

    DetachFromViewport();

    if (ActiveViewport.IsValid() && OverlayWidget.IsValid())
    {
        ActiveViewport->AddOverlayWidget(OverlayWidget.ToSharedRef(), 450);
        AttachedViewport = ActiveViewport;
    }
}

void FWeaverViewportOverlay::DetachFromViewport()
{
    if (AttachedViewport.IsValid()) { OnDeactivated.ExecuteIfBound(); }
    if (const TSharedPtr<IAssetViewport> Viewport = AttachedViewport.Pin())
    {
        if (OverlayWidget.IsValid())
        {
            Viewport->RemoveOverlayWidget(OverlayWidget.ToSharedRef());
        }
    }

    AttachedViewport.Reset();
}

#include "WeaverViewportOverlay.h"

#include "IAssetViewport.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "SWeaverTimelineHost.h"
#include "Widgets/SOverlay.h"

FWeaverViewportOverlay::~FWeaverViewportOverlay()
{
    Unregister();
    bVisible = true;
}

void FWeaverViewportOverlay::Register(
    const TSharedRef<SWidget>& InContent,
    const FText& InCollapsedLabel,
    const float InExpandedHeight)
{
    Unregister();

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
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }

    DetachFromViewport();
    OverlayWidget.Reset();
}

void FWeaverViewportOverlay::SetVisible(const bool bInVisible)
{
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
    if (const TSharedPtr<IAssetViewport> Viewport = AttachedViewport.Pin())
    {
        if (OverlayWidget.IsValid())
        {
            Viewport->RemoveOverlayWidget(OverlayWidget.ToSharedRef());
        }
    }

    AttachedViewport.Reset();
}

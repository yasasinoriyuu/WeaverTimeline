#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"

class IAssetViewport;
class SWidget;
class SWeaverEditableTimeline;

/**
 * Attaches arbitrary orchestration UI to the bottom of the active Level Editor viewport.
 * Extracted from CAK's proven overlay placement. It only discovers viewport changes;
 * it does not poll business data or own Sequencer state.
 */
class FWeaverViewportOverlay
{
public:
    ~FWeaverViewportOverlay();

    void Register(
        const TSharedRef<SWidget>& InContent,
        const FText& InCollapsedLabel,
        float InExpandedHeight = 300.0f,
        FSimpleDelegate InOnDeactivated = FSimpleDelegate());
    /** Preferred editable path; hide, collapse, viewport detach and unregister cancel active edits. */
    void RegisterEditable(const TSharedRef<SWeaverEditableTimeline>& InContent,
        const FText& InCollapsedLabel, float InExpandedHeight = 300.0f);
    void Unregister();
    void SetVisible(bool bInVisible);

    bool IsRegistered() const { return OverlayWidget.IsValid(); }
    bool IsVisible() const { return bVisible; }

private:
    bool Tick(float DeltaTime);
    void AttachToActiveViewport();
    void DetachFromViewport();

    TSharedPtr<SWidget> OverlayWidget;
    TWeakPtr<IAssetViewport> AttachedViewport;
    FTSTicker::FDelegateHandle TickerHandle;
    bool bVisible = true;
    FSimpleDelegate OnDeactivated;
};

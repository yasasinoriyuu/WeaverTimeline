#include "WeaverSequencerSection.h"

#include "SWeaverEditableTimeline.h"
#include "MovieSceneSection.h"
#include "SequencerSectionPainter.h"
#include "Widgets/SOverlay.h"
#include "ISequencer.h"

FWeaverSequencerSection::FWeaverSequencerSection(UMovieSceneSection& Section,
    TSharedRef<SWeaverEditableTimeline> InTimeline, TWeakPtr<ISequencer> InOwner)
    : FSequencerSection(Section), Timeline(InTimeline), Owner(InOwner)
{
    ensureMsgf(Section.GetRange() == TRange<FFrameNumber>::All() && Section.IsLocked(),
        TEXT("Weaver native host requires a consumer-owned all-time locked layout section."));
}

FWeaverSequencerSection::~FWeaverSequencerSection()
{
    Timeline->Shutdown();
    if (auto Overlay = MountedOverlay.Pin()) { Overlay->RemoveSlot(Timeline); }
}

void FWeaverSequencerSection::CreateViewWidgets(const UE::Sequencer::FCreateSectionViewWidgetParams& Params)
{
    if (MountedOverlay.Pin() == Params.Overlay) { return; }
    Timeline->Deactivate(); // Reparenting is an edit boundary, never a pending commit.
    if (auto Previous = MountedOverlay.Pin()) { Previous->RemoveSlot(Timeline); }
    MountedOverlay = Params.Overlay;
    Params.Overlay->AddSlot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Timeline];
}

int32 FWeaverSequencerSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
    return Painter.LayerId;
}

float FWeaverSequencerSection::GetSectionHeight(const UE::Sequencer::FViewDensityInfo&) const
{
    return FMath::Max(36.f, Timeline->GetTimeline()->ComputeDesiredSize(1.f).Y);
}

void FWeaverSequencerSection::Tick(const FGeometry&, const FGeometry&, double, float)
{
    const float Height = FMath::Max(36.f, Timeline->GetTimeline()->ComputeDesiredSize(1.f).Y);
    if (!FMath::IsNearlyEqual(Height, LastHeight))
    {
        const bool bWasLaidOut = LastHeight > 0;
        LastHeight = Height;
        if (auto Sequencer = Owner.Pin(); bWasLaidOut && Sequencer)
        { Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately); }
    }
}

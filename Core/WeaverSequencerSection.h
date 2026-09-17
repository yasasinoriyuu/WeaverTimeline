#pragma once

#include "ISequencerSection.h"

class SOverlay;
class SWeaverEditableTimeline;
class ISequencer;

/** Public Sequencer section-view integration extracted from CAK's SectionTrack path.
 * The consumer owns the root track and an all-time, locked layout section.
 * No business assets, track registration, runtime evaluation or auto-creation belong here.
 */
class FWeaverSequencerSection : public FSequencerSection
{
public:
    FWeaverSequencerSection(UMovieSceneSection& Section, TSharedRef<SWeaverEditableTimeline> InTimeline,
        TWeakPtr<ISequencer> InOwner);
    virtual ~FWeaverSequencerSection();
    virtual void CreateViewWidgets(const UE::Sequencer::FCreateSectionViewWidgetParams& Params) override;
    virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;
    virtual FText GetSectionTitle() const override { return FText::GetEmpty(); }
    virtual float GetSectionHeight(const UE::Sequencer::FViewDensityInfo& Density) const override;
    virtual void Tick(const FGeometry& Allotted, const FGeometry& Clipped, double Time, float Delta) override;
    virtual bool SectionIsResizable() const override { return false; }
    // Lock the native anchor, not the child widget's input path.
    virtual bool IsReadOnly() const override { return false; }
    virtual bool IsDeletable() const override { return false; }
    TSharedRef<SWeaverEditableTimeline> GetTimeline() const { return Timeline; }
private:
    TSharedRef<SWeaverEditableTimeline> Timeline;
    TWeakPtr<SOverlay> MountedOverlay;
    TWeakPtr<ISequencer> Owner;
    float LastHeight = 0;
};

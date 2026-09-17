#pragma once

#include "MovieSceneTrackEditor.h"

class FWeaverReferenceTrackEditor : public FMovieSceneTrackEditor
{
public:
    using FMovieSceneTrackEditor::FMovieSceneTrackEditor;
    static TSharedRef<ISequencerTrackEditor> Create(TSharedRef<ISequencer> Sequencer);
    virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
    virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& Section, UMovieSceneTrack& Track, FGuid Binding) override;
    virtual void BuildAddTrackMenu(FMenuBuilder& Menu) override;
private:
    void AddReferenceTrack();
};

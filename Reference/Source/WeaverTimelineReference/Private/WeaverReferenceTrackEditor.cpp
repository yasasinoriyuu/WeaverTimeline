#include "WeaverReferenceTrackEditor.h"
#include "WeaverReferenceTrack.h"
#include "WeaverSequencerSection.h"
#include "SWeaverEditableTimeline.h"
#include "WeaverTimelineReferenceAdapter.h"
#include "ISequencer.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"

TSharedRef<ISequencerTrackEditor> FWeaverReferenceTrackEditor::Create(TSharedRef<ISequencer> Sequencer)
{
    return MakeShared<FWeaverReferenceTrackEditor>(Sequencer);
}

bool FWeaverReferenceTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
    return Type == UWeaverReferenceTrack::StaticClass();
}

TSharedRef<ISequencerSection> FWeaverReferenceTrackEditor::MakeSectionInterface(
    UMovieSceneSection& Section, UMovieSceneTrack&, FGuid)
{
    auto& Reference = *CastChecked<UWeaverReferenceSection>(&Section);
    auto Adapter = MakeShared<FWeaverTimelineReferenceAdapter>(*Reference.Document);
    auto Timeline = SNew(SWeaverEditableTimeline).Adapter(Adapter).EmbeddedInSequencer(true).SequencerSource(GetSequencer());
    return MakeShared<FWeaverSequencerSection>(Section, Timeline, GetSequencer());
}

void FWeaverReferenceTrackEditor::BuildAddTrackMenu(FMenuBuilder& Menu)
{
    Menu.AddMenuEntry(NSLOCTEXT("WeaverReference", "Add", "Weaver 编排参考"),
        NSLOCTEXT("WeaverReference", "AddTip", "添加嵌入当前 Sequencer 的参考编排轨道"), FSlateIcon(),
        FUIAction(FExecuteAction::CreateSP(this, &FWeaverReferenceTrackEditor::AddReferenceTrack)));
}

void FWeaverReferenceTrackEditor::AddReferenceTrack()
{
    auto Owner = GetSequencer();
    auto* Sequence = Owner ? Owner->GetFocusedMovieSceneSequence() : nullptr;
    auto* Scene = Sequence ? Sequence->GetMovieScene() : nullptr;
    if (!Scene || Scene->IsReadOnly() || Scene->FindTrack<UWeaverReferenceTrack>()) { return; }
    FScopedTransaction Transaction(NSLOCTEXT("WeaverReference", "AddTransaction", "添加参考编排"));
    Scene->Modify();
    auto* Track = Scene->AddTrack<UWeaverReferenceTrack>();
    Track->AddSection(*Track->CreateNewSection());
    Owner->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

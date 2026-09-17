#include "WeaverReferenceTrack.h"

UWeaverReferenceSection::UWeaverReferenceSection()
{
    SetRange(TRange<FFrameNumber>::All());
    SetIsLocked(true);
    Document = CreateDefaultSubobject<UWeaverReferenceDocument>(TEXT("Document"));
    Document->SetFlags(RF_Transactional);
    Document->LaneId = FGuid::NewGuid();
    FWeaverReferenceItem Block;
    Block.Id = FGuid::NewGuid();
    Document->Items.Add(Block);
}

UMovieSceneSection* UWeaverReferenceTrack::CreateNewSection()
{
    return NewObject<UWeaverReferenceSection>(this, NAME_None, RF_Transactional);
}

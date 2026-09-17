#pragma once

#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "WeaverReferenceDocument.h"
#include "WeaverReferenceTrack.generated.h"

/** Reference consumer only: no reflection classes are copied into Core. */
UCLASS()
class UWeaverReferenceSection : public UMovieSceneSection
{
    GENERATED_BODY()
public:
    UWeaverReferenceSection();
    UPROPERTY(Instanced) TObjectPtr<UWeaverReferenceDocument> Document;
};

UCLASS()
class UWeaverReferenceTrack : public UMovieSceneTrack
{
    GENERATED_BODY()
public:
    virtual bool SupportsType(TSubclassOf<UMovieSceneSection> Type) const override { return Type == UWeaverReferenceSection::StaticClass(); }
    virtual UMovieSceneSection* CreateNewSection() override;
    virtual const TArray<UMovieSceneSection*>& GetAllSections() const override { return Sections; }
    virtual bool HasSection(const UMovieSceneSection& Section) const override { return Sections.Contains(&Section); }
    virtual bool IsEmpty() const override { return Sections.IsEmpty(); }
    virtual void AddSection(UMovieSceneSection& Section) override { Sections.AddUnique(&Section); }
    virtual void RemoveSection(UMovieSceneSection& Section) override { Sections.Remove(&Section); }
    virtual void RemoveSectionAt(int32 Index) override { Sections.RemoveAt(Index); }
    virtual void RemoveAllAnimationData() override { Sections.Reset(); }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("WeaverReference", "RootTrack", "Weaver 编排参考"); }
private:
    UPROPERTY() TArray<UMovieSceneSection*> Sections;
};

#include "WeaverReferenceDocument.h"

void UWeaverReferenceDocument::PostEditUndo()
{
    Super::PostEditUndo();
    OnChanged.Broadcast();
}

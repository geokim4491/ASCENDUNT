#include "AscInteractable.h"

namespace
{
    FAscInteractionCompletedNative GAscInteractionCompleted;
}

FAscInteractionCompletedNative& FAscInteractionEvents::OnInteractionCompleted()
{
    return GAscInteractionCompleted;
}

void FAscInteractionEvents::BroadcastInteractionCompleted(
    UWorld* World,
    AActor* Target,
    AHeroCharacter* Interactor)
{
    GAscInteractionCompleted.Broadcast(World, Target, Interactor);
}

bool IAscInteractable::CanInteract_Implementation(AHeroCharacter* Interactor) const
{
    return true;
}

FAscInteractionOption IAscInteractable::GetInteractionOption_Implementation(AHeroCharacter* Interactor) const
{
    return FAscInteractionOption();
}

void IAscInteractable::BeginInteraction_Implementation(AHeroCharacter* Interactor)
{
}

void IAscInteractable::CancelInteraction_Implementation(AHeroCharacter* Interactor)
{
}

void IAscInteractable::CompleteInteraction_Implementation(AHeroCharacter* Interactor)
{
}

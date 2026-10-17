#include "ASCENDUNT/Mission/Conditions/AscActorDestroyedConditionComponent.h"

#include "GameFramework/Actor.h"

void UAscActorDestroyedConditionComponent::OnActivated()
{
    if (!IsValid(TargetActor))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Mission] ActorDestroyedConditionComponent has no valid TargetActor."));
        Fail();
        return;
    }

    TargetActor->OnDestroyed.RemoveDynamic(this, &UAscActorDestroyedConditionComponent::HandleTargetDestroyed);
    TargetActor->OnDestroyed.AddDynamic(this, &UAscActorDestroyedConditionComponent::HandleTargetDestroyed);
}

void UAscActorDestroyedConditionComponent::OnDeactivated()
{
    if (IsValid(TargetActor))
    {
        TargetActor->OnDestroyed.RemoveDynamic(this, &UAscActorDestroyedConditionComponent::HandleTargetDestroyed);
    }
}

void UAscActorDestroyedConditionComponent::OnReset()
{
}

void UAscActorDestroyedConditionComponent::HandleTargetDestroyed(AActor* DestroyedActor)
{
    Satisfy();
}

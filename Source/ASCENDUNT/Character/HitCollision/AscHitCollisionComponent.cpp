#include "AscHitCollisionComponent.h"

#include "GameFramework/Actor.h"

UAscHitCollisionComponent::UAscHitCollisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Hit response policy is centralized in the project collision profile.
	// Blueprint authors only place/resize the component and choose HitZone.
	SetCollisionProfileName(TEXT("AscHitCollision"));
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetGenerateOverlapEvents(true);

	SetCanEverAffectNavigation(false);
	CanCharacterStepUpOn = ECB_No;
	SetIsReplicated(false);

	SetHiddenInGame(true);
}

void UAscHitCollisionComponent::SetHitCollisionsEnabled(AActor* OwnerActor, bool bEnabled)
{
	if (!OwnerActor)
	{
		return;
	}

	TInlineComponentArray<UAscHitCollisionComponent*> HitCollisions;
	OwnerActor->GetComponents(HitCollisions);

	for (UAscHitCollisionComponent* HitCollision : HitCollisions)
	{
		if (!IsValid(HitCollision))
		{
			continue;
		}

		HitCollision->SetCollisionEnabled(
			bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

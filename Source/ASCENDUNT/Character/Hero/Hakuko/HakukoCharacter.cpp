#include "HakukoCharacter.h"

#include "ASCENDUNT/Character/Hero/Hakuko/HakukoSkillComponent.h"
#include "ASCENDUNT/Character/Hero/Hakuko/HakukoFoxComponent.h"

AHakukoCharacter::AHakukoCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HakukoSkillComponent = CreateDefaultSubobject<UHakukoSkillComponent>(TEXT("HakukoSkillComponent"));
}

UHakukoFoxComponent* AHakukoCharacter::GetHakukoFoxComponent() const
{
	return FindComponentByClass<UHakukoFoxComponent>();
}

bool AHakukoCharacter::GetWeaponAOEParameters(float& OutRadius, float& OutDamageCoefficient, bool& bOutDebugDrawRadius, float& OutDebugDrawDuration) const
{
	OutRadius = 0.f;
	OutDamageCoefficient = 1.f;
	bOutDebugDrawRadius = false;
	OutDebugDrawDuration = 0.f;
	return HakukoSkillComponent && HakukoSkillComponent->GetUltimateAOEParameters(
		OutRadius,
		OutDamageCoefficient,
		bOutDebugDrawRadius,
		OutDebugDrawDuration);
}

void AHakukoCharacter::InitializeGameplayBindings()
{
	Super::InitializeGameplayBindings();
	if (HakukoSkillComponent)
	{
		HakukoSkillComponent->InitializeForGameplay();
	}
}

void AHakukoCharacter::UninitializeGameplayBindings()
{
	if (HakukoSkillComponent)
	{
		HakukoSkillComponent->DeinitializeForGameplay();
	}
	Super::UninitializeGameplayBindings();
}

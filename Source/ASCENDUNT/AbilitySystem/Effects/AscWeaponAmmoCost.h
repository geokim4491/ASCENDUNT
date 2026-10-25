#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayModMagnitudeCalculation.h"
#include "AscWeaponAmmoCost.generated.h"

UCLASS()
class ASCENDUNT_API UAscWeaponAmmoCostMagnitude : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UAscWeaponAmmoCostMagnitude();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition AmmoCostCapture;
};

/** Instant modifiers participate in GAS delta prediction; executions do not. */
UCLASS()
class ASCENDUNT_API UAscWeaponAmmoCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAscWeaponAmmoCost();
};

/** Authoritative refunds and existing server-only skill ammo transactions. */
UCLASS()
class ASCENDUNT_API UAscWeaponAmmoAdjustment : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAscWeaponAmmoAdjustment();
};

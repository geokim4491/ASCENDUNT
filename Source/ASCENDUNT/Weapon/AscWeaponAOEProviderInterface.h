#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AscWeaponAOEProviderInterface.generated.h"

UINTERFACE(MinimalAPI)
class UAscWeaponAOEProviderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Optional avatar-side hook used by generic weapon abilities.
 * The weapon code knows only whether the current shot should explode; hero-specific state stays on the hero.
 */
class ASCENDUNT_API IAscWeaponAOEProviderInterface
{
	GENERATED_BODY()

public:
	virtual bool GetWeaponAOEParameters(float& OutRadius, float& OutDamageCoefficient, bool& bOutDebugDrawRadius, float& OutDebugDrawDuration) const = 0;
};

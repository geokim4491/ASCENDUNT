#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Weapon/GA_Melee.h"
#include "GA_AstellMelee.generated.h"

/** Astell-specific melee ability. */
UCLASS()
class ASCENDUNT_API UGA_AstellMelee : public UGA_Melee
{
	GENERATED_BODY()

public:
	UGA_AstellMelee();
};

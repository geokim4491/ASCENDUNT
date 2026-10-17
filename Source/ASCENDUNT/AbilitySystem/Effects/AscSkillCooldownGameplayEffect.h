#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AscSkillCooldownGameplayEffect.generated.h"

/**
 * Shared duration-only GameplayEffect used by DataAbility cooldowns.
 * The actual duration and granted cooldown tag are written to the spec at runtime.
 */
UCLASS()
class ASCENDUNT_API UAscSkillCooldownGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAscSkillCooldownGameplayEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

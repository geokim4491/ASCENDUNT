#include "AscSkillCooldownGameplayEffect.h"

UAscSkillCooldownGameplayEffect::UAscSkillCooldownGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
}

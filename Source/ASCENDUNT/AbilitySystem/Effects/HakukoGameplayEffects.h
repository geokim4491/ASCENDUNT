#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HakukoGameplayEffects.generated.h"

/** Default duration GE for Hakuko's stimulant. Values come from SetByCaller tags. */
UCLASS()
class ASCENDUNT_API UHakukoStimGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHakukoStimGameplayEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Default duration GE for Hakuko's ultimate outgoing-damage multiplier. */
UCLASS()
class ASCENDUNT_API UHakukoUltimateGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHakukoUltimateGameplayEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

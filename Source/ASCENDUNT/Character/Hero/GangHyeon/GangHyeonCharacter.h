#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "GangHyeonCharacter.generated.h"

UCLASS()
class ASCENDUNT_API AGangHyeonCharacter : public AHeroCharacter
{
	GENERATED_BODY()

public:
	AGangHyeonCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

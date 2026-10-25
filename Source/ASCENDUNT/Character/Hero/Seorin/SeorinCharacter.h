#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "SeorinCharacter.generated.h"

UCLASS()
class ASCENDUNT_API ASeorinCharacter : public AHeroCharacter
{
	GENERATED_BODY()

public:
	ASeorinCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

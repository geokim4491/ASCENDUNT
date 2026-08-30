#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "AscHeroRosterData.generated.h"

/**
 * Shared hero roster used by match, training range, and future modes.
 *
 * Hero selection should not store its HeroId -> PawnData table in one specific
 * GameMode. MatchGameMode, TrainingGameMode, and UI can all point at the same
 * roster asset so the selectable hero list stays consistent.
 */
UCLASS(BlueprintType, Const)
class ASCENDUNT_API UAscHeroRosterData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Hero Selection")
    TArray<FAscHeroDefinition> HeroDefinitions;

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Hero Selection")
    FAscHeroDefinition GetHeroDefinitionByIndex(int32 Index) const;

    const FAscHeroDefinition* FindHeroDefinition(int32 HeroId) const;
    const FAscHeroDefinition* FindHeroDefinitionByPawnClass(const UClass* PawnClass) const;
    bool HasHeroDefinitions() const { return HeroDefinitions.Num() > 0; }
};

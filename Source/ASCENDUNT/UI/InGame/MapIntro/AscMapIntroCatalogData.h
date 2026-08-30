#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AscMapIntroCatalogData.generated.h"

class UTexture2D;

/** Character-selection presentation selected from the local player's map side. */
USTRUCT(BlueprintType)
struct FAscCharacterSelectSidePresentation
{
    GENERATED_BODY()

    /** Team treated as the attacking side for this map's initial character selection. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Character Selection", meta = (ClampMin = "1"))
    uint8 AttackingTeamId = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<UTexture2D> AttackerBackgroundImage = nullptr;

    /** Team treated as the defending side for this map's initial character selection. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Character Selection", meta = (ClampMin = "1"))
    uint8 DefendingTeamId = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<UTexture2D> DefenderBackgroundImage = nullptr;
};

/** Static client-side presentation data for one map. */
USTRUCT(BlueprintType)
struct FAscMapIntroPresentation
{
    GENERATED_BODY()

    /** Short map package name. Example: L_Arena01 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Map Intro")
    FName MapId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Map Intro")
    FText MapDisplayName;

    /** Full-screen background used during the MapIntro phase. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Map Intro")
    TObjectPtr<UTexture2D> BackgroundImage = nullptr;

    /** Attack/defense-specific background used during the initial character-selection phase. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    FAscCharacterSelectSidePresentation CharacterSelectPresentation;
};

/**
 * Client-side map presentation lookup table.
 * SelectedMapPath and local TeamId are already replicated, so textures remain local assets.
 */
UCLASS(BlueprintType)
class ASCENDUNT_API UAscMapIntroCatalogData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Map Intro")
    bool FindPresentation(const FString& MapPathOrName, FAscMapIntroPresentation& OutPresentation) const;

    /** Returns the initial character-selection image for the requested map/team, or nullptr if not configured. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Character Selection")
    UTexture2D* GetCharacterSelectImageForTeam(const FString& MapPathOrName, uint8 TeamId) const;

    const FAscMapIntroPresentation* FindPresentationNative(const FString& MapPathOrName) const;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Map Intro")
    TArray<FAscMapIntroPresentation> Maps;
};

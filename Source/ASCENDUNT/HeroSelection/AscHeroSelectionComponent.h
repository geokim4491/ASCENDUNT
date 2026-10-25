#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "AscHeroSelectionComponent.generated.h"

class AAscenduntPlayerController;
class AAscenduntPlayerState;
class UAscHeroRosterData;
class UHeroPawnData;

/**
 * Common hero-selection implementation shared by match/training/future modes.
 *
 * This component owns the reusable part of hero selection:
 * - HeroId validity
 * - HeroId -> role / PawnData resolution
 * - duplicate hero / duplicate role rule checks
 * - PlayerState confirm state + PawnData assignment
 *
 * GameModes still decide mode-specific policy such as "is this phase allowed?"
 * and "should the pawn be replaced immediately after confirm?".
 */
UCLASS(ClassGroup = (ASCENDUNT), meta = (BlueprintSpawnableComponent))
class ASCENDUNT_API UAscHeroSelectionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAscHeroSelectionComponent();

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Hero Selection")
    UAscHeroRosterData* GetHeroRosterData() const { return HeroRosterData; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Hero Selection")
    void SetHeroRosterData(UAscHeroRosterData* InHeroRosterData) { HeroRosterData = InHeroRosterData; }

    bool CanConfirmHero(AAscenduntPlayerController* PlayerController, int32 HeroId, const FAscHeroSelectionRules& Rules, EAscHeroSelectionRejectReason& OutRejectReason) const;
    bool ConfirmHero(AAscenduntPlayerController* PlayerController, int32 HeroId, EAscHeroSelectionContext Context, const FAscHeroSelectionRules& Rules, EAscHeroSelectionRejectReason& OutRejectReason);

    const FAscHeroDefinition* FindHeroDefinition(int32 HeroId) const;
    bool IsHeroIdValid(int32 HeroId) const;
    EAscHeroRole ResolveHeroRole(int32 HeroId) const;
    const UHeroPawnData* ResolvePawnDataForHero(int32 HeroId) const;

protected:
    /** Shared selectable hero list. Configure this on the component in BP_MatchGameMode / BP_TrainingGameMode. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Hero Selection")
    TObjectPtr<UAscHeroRosterData> HeroRosterData = nullptr;

private:
    bool WouldViolateDuplicateHeroRule(const AAscenduntPlayerState* RequestingPlayerState, int32 HeroId, const FAscHeroSelectionRules& Rules) const;
    bool WouldViolateDuplicateRoleRule(const AAscenduntPlayerState* RequestingPlayerState, EAscHeroRole HeroRole, const FAscHeroSelectionRules& Rules) const;
    bool ShouldCompareForDuplicateRules(const AAscenduntPlayerState* RequestingPlayerState, const AAscenduntPlayerState* OtherPlayerState, const FAscHeroSelectionRules& Rules) const;
};

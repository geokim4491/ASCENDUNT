#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/AscenduntGameplayAbility.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "GA_OpenCharacterSelection.generated.h"

/**
 * Local UI ability for opening the character selection screen from gameplay input.
 *
 * Intended usage:
 * - Map H to InputTag.UI.CharacterSelection in the hero InputConfig.
 * - Grant this ability from a common/player AbilitySet with the same input tag.
 *
 * Server authority is enforced by PlayerController::Server_RequestOpenRespawnRoomHeroSelection()
 * and MatchGameMode. This ability only converts InputTag activation into a server request.
 */
UCLASS()
class ASCENDUNT_API UGA_OpenCharacterSelection : public UAscenduntGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_OpenCharacterSelection(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
    //~UGameplayAbility interface
    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags) const override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
    //~End of UGameplayAbility interface
};

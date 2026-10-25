#include "GA_OpenCharacterSelection.h"

#include "ASCENDUNT/Player/AscenduntPlayerController.h"

UGA_OpenCharacterSelection::UGA_OpenCharacterSelection(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
    NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;

    ActivationPolicy = EAscenduntAbilityActivationPolicy::OnInputTriggered;
    ActivationGroup = EAscenduntAbilityActivationGroup::Independent;
}

bool UGA_OpenCharacterSelection::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    if (!ActorInfo)
    {
        return false;
    }

    if (!ActorInfo->IsLocallyControlled())
    {
        return false;
    }

    return Cast<AAscenduntPlayerController>(ActorInfo->PlayerController.Get()) != nullptr;
}

void UGA_OpenCharacterSelection::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AAscenduntPlayerController* AscPC = ActorInfo ? Cast<AAscenduntPlayerController>(ActorInfo->PlayerController.Get()) : nullptr;
    if (!AscPC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (AscPC->IsHeroSelectionOpen())
    {
        AscPC->CloseHeroSelection();
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // H/InputTag is only a request trigger. The server validates Status.SpawnRoom
    // and opens the UI through Client_OpenHeroSelection only if the request is allowed.
    AscPC->Server_RequestOpenRespawnRoomHeroSelection();

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

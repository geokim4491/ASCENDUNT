#include "GA_Interact.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Interaction/AscInteractable.h"
#include "ASCENDUNT/Interaction/AscInteractionComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"

UGA_Interact::UGA_Interact(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ActivationPolicy = EAscenduntAbilityActivationPolicy::OnInputTriggered;
    ActivationGroup = EAscenduntAbilityActivationGroup::Exclusive_Replaceable;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;

    AbilityTags.AddTag(AscenduntGameplayTags::Ability_Interact);
    ActivationOwnedTags.AddTag(AscenduntGameplayTags::Status_Interacting);
}

bool UGA_Interact::CanActivateAbility(
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

    AActor* Target = nullptr;
    FAscInteractionOption Option;
    return ResolveInteractionTarget(ActorInfo, Target, Option);
}

void UGA_Interact::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    ActiveTarget = nullptr;
    ActiveOption = FAscInteractionOption();
    InteractionStartTime = 0.0;
    bServerInteractionBegun = false;
    bCompleted = false;
    bMovementLockApplied = false;

    AActor* ResolvedTarget = nullptr;
    FAscInteractionOption ResolvedOption;
    if (!ResolveInteractionTarget(ActorInfo, ResolvedTarget, ResolvedOption))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ActiveTarget = ResolvedTarget;
    ActiveOption = ResolvedOption;
    InteractionStartTime = GetCurrentWorldTimeSeconds();

    AHeroCharacter* Hero = ActorInfo ? Cast<AHeroCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAscInteractionComponent* Interaction = Hero ? Hero->GetInteractionComponent() : nullptr;

    if (ActorInfo && ActorInfo->IsLocallyControlled() && Interaction)
    {
        Interaction->BeginLocalInteraction(ActiveTarget, ActiveOption);
    }

    if (ActiveOption.bLockMovement)
    {
        SetMovementLocked(true);
    }

    if (ActorInfo && ActorInfo->IsNetAuthority())
    {
        IAscInteractable::Execute_BeginInteraction(ActiveTarget, Hero);
        bServerInteractionBegun = true;
    }

    if (ActiveOption.InputMode == EAscInteractionInputMode::Pressed)
    {
        CompleteInteraction();
        return;
    }

    ScheduleInteractionTick();
}

void UGA_Interact::InputReleased(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputReleased(Handle, ActorInfo, ActivationInfo);

    // This project already uses direct release forwarding for hold abilities.
    if (UAscenduntAbilitySystemComponent* AscASC = GetAscenduntASCFromActorInfo())
    {
        if (ActorInfo && ActorInfo->IsLocallyControlled() && !ActorInfo->IsNetAuthority())
        {
            AscASC->ServerSetInputReleased(Handle);
        }
    }

    if (ActiveOption.InputMode == EAscInteractionInputMode::Hold && !bCompleted)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    }
}

void UGA_Interact::HandleInteractionTick()
{
    if (!IsActive() || !CurrentActorInfo)
    {
        return;
    }

    FAscInteractionOption CurrentOption;
    if (!ValidateActiveTarget(&CurrentOption))
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // Duration is fixed at BeginInteraction so a target cannot unexpectedly extend or
    // shorten an already-started hold. Range/FOV/CanInteract are still revalidated.
    const double Elapsed = FMath::Max(0.0, GetCurrentWorldTimeSeconds() - InteractionStartTime);
    const float Duration = FMath::Max(0.01f, ActiveOption.HoldDuration);
    const float Progress = FMath::Clamp(static_cast<float>(Elapsed / Duration), 0.0f, 1.0f);

    if (CurrentActorInfo->IsLocallyControlled())
    {
        if (AHeroCharacter* Hero = Cast<AHeroCharacter>(CurrentActorInfo->AvatarActor.Get()))
        {
            if (UAscInteractionComponent* Interaction = Hero->GetInteractionComponent())
            {
                Interaction->UpdateLocalInteractionProgress(Progress);
            }
        }
    }

    if (Progress >= 1.0f)
    {
        CompleteInteraction();
        return;
    }

    ScheduleInteractionTick();
}

bool UGA_Interact::ResolveInteractionTarget(
    const FGameplayAbilityActorInfo* ActorInfo,
    AActor*& OutTarget,
    FAscInteractionOption& OutOption) const
{
    OutTarget = nullptr;
    OutOption = FAscInteractionOption();

    const AHeroCharacter* Hero = ActorInfo ? Cast<AHeroCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    UAscInteractionComponent* Interaction = Hero ? Hero->GetInteractionComponent() : nullptr;
    if (!Interaction)
    {
        return false;
    }

    OutTarget = Interaction->FindBestInteractable(&OutOption);
    return IsValid(OutTarget);
}

bool UGA_Interact::ValidateActiveTarget(FAscInteractionOption* OutCurrentOption) const
{
    const AHeroCharacter* Hero = CurrentActorInfo
        ? Cast<AHeroCharacter>(CurrentActorInfo->AvatarActor.Get())
        : nullptr;
    UAscInteractionComponent* Interaction = Hero ? Hero->GetInteractionComponent() : nullptr;

    return Interaction && IsValid(ActiveTarget) &&
        Interaction->ValidateInteractable(ActiveTarget, OutCurrentOption);
}

void UGA_Interact::CompleteInteraction()
{
    if (bCompleted || !CurrentActorInfo || !IsValid(ActiveTarget))
    {
        return;
    }

    if (!ValidateActiveTarget())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    bCompleted = true;

    if (CurrentActorInfo->IsLocallyControlled())
    {
        if (AHeroCharacter* Hero = Cast<AHeroCharacter>(CurrentActorInfo->AvatarActor.Get()))
        {
            if (UAscInteractionComponent* Interaction = Hero->GetInteractionComponent())
            {
                Interaction->UpdateLocalInteractionProgress(1.0f);
            }
        }
    }

    if (CurrentActorInfo->IsNetAuthority())
    {
        AHeroCharacter* Hero = Cast<AHeroCharacter>(CurrentActorInfo->AvatarActor.Get());
        AActor* CompletedTarget = ActiveTarget;
        UWorld* InteractionWorld = GetWorld();

        IAscInteractable::Execute_CompleteInteraction(CompletedTarget, Hero);

        // Mission conditions and other observers only care about an authoritative
        // interaction that reached the common completion point.
        FAscInteractionEvents::BroadcastInteractionCompleted(
            InteractionWorld,
            CompletedTarget,
            Hero);
    }

    const bool bReplicateSuccessfulEnd = CurrentActorInfo->IsNetAuthority();
    EndAbility(
        CurrentSpecHandle,
        CurrentActorInfo,
        CurrentActivationInfo,
        bReplicateSuccessfulEnd,
        false);
}

void UGA_Interact::ScheduleInteractionTick()
{
    if (InteractionTickTask)
    {
        InteractionTickTask->EndTask();
        InteractionTickTask = nullptr;
    }

    InteractionTickTask = UAbilityTask_WaitDelay::WaitDelay(this, ValidationInterval);
    if (InteractionTickTask)
    {
        InteractionTickTask->OnFinish.AddDynamic(this, &ThisClass::HandleInteractionTick);
        InteractionTickTask->ReadyForActivation();
    }
}

void UGA_Interact::SetMovementLocked(bool bLocked)
{
    if (!CurrentActorInfo || bMovementLockApplied == bLocked)
    {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(CurrentActorInfo->PlayerController.Get());
    if (!PC)
    {
        return;
    }

    PC->SetIgnoreMoveInput(bLocked);
    bMovementLockApplied = bLocked;

    if (bLocked)
    {
        if (APawn* Pawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get()))
        {
            if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
            {
                Movement->StopMovementImmediately();
            }
        }
    }
}

double UGA_Interact::GetCurrentWorldTimeSeconds() const
{
    const AActor* Avatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
    const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
    return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}

void UGA_Interact::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    if (InteractionTickTask)
    {
        InteractionTickTask->EndTask();
        InteractionTickTask = nullptr;
    }

    if (ActorInfo && ActorInfo->IsNetAuthority() && bServerInteractionBegun && !bCompleted && IsValid(ActiveTarget))
    {
        AHeroCharacter* Hero = Cast<AHeroCharacter>(ActorInfo->AvatarActor.Get());
        IAscInteractable::Execute_CancelInteraction(ActiveTarget, Hero);
    }

    if (ActorInfo && ActorInfo->IsLocallyControlled())
    {
        if (AHeroCharacter* Hero = Cast<AHeroCharacter>(ActorInfo->AvatarActor.Get()))
        {
            if (UAscInteractionComponent* Interaction = Hero->GetInteractionComponent())
            {
                Interaction->EndLocalInteraction();
            }
        }
    }

    if (bMovementLockApplied)
    {
        SetMovementLocked(false);
    }

    ActiveTarget = nullptr;
    ActiveOption = FAscInteractionOption();
    InteractionStartTime = 0.0;
    bServerInteractionBegun = false;
    bCompleted = false;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

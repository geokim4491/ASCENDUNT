#include "GA_FireControlBase.h"
#include "GA_FireShot.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"

UGA_FireControlBase::UGA_FireControlBase()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	bReplicateInputDirectly = false;
}

bool UGA_FireControlBase::MatchesShotClass(const UClass* Class) const
{
	return ShotAbilityClass && ShotAbilityClass.Get() == Class;
}

bool UGA_FireControlBase::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(GetSourceObject(Handle, ActorInfo));
	return ActorInfo && Weapon && !Weapon->IsReloading() && Weapon->GetOwner() == ActorInfo->AvatarActor.Get()
	       && Weapon->GetCachedAscASC() == ActorInfo->AbilitySystemComponent.Get() && ShotAbilityClass;
}

void UGA_FireControlBase::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bInputReleased = false;
	AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(GetSourceObject(Handle, ActorInfo));
	if (!Weapon || !ShotAbilityClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Weapon->SetActiveFireControl(this);
	ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleControlReleased);
	ReleaseTask->ReadyForActivation();
}

void UGA_FireControlBase::InputReleased(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo)
{
	// ASC broadcasts the generic event after this callback. WaitInputRelease owns the release RPC.
}

void UGA_FireControlBase::HandleControlReleased(float TimeHeld)
{
	if (!IsActive() || bInputReleased)
	{
		return;
	}

	bInputReleased = true;
	OnControlReleased();
}

bool UGA_FireControlBase::RequestShot()
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsLocallyControlled())
	{
		return false;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const UObject* Weapon = GetCurrentSourceObject();
	if (!ASC || !Weapon || !ShotAbilityClass)
	{
		return false;
	}

	FGameplayAbilitySpecHandle ShotHandle;
	{
		FScopedAbilityListLock AbilityListLock(*ASC);
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetClass() == ShotAbilityClass.Get() && Spec.SourceObject.Get() == Weapon)
			{
				ShotHandle = Spec.Handle;
				break;
			}
		}
	}

	return ShotHandle.IsValid() && ASC->TryActivateAbility(ShotHandle, false);
}

void UGA_FireControlBase::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ReleaseTask)
	{
		ReleaseTask->EndTask();
		ReleaseTask = nullptr;
	}

	if (AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(GetSourceObject(Handle, ActorInfo)))
	{
		if (Weapon->GetActiveFireControl() == this)
		{
			Weapon->SetActiveFireControl(nullptr);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

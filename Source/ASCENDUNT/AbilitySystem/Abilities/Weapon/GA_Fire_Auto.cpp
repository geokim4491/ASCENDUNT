#include "GA_Fire_Auto.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"

UGA_Fire_Auto::UGA_Fire_Auto()
{
	ActivationPolicy = EAscenduntAbilityActivationPolicy::WhileInputActive;
}

void UGA_Fire_Auto::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (IsActive() && ActorInfo->IsLocallyControlled())
	{
		HandleTickFire();
	}
}

void UGA_Fire_Auto::HandleTickFire()
{
	if (!IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsLocallyControlled() || bInputReleased)
	{
		return;
	}

	UAbilitySystemComponent* ASC = nullptr;
	UAscenduntAbilitySystemComponent* AscASC = nullptr;
	AAscenduntWeapon* Weapon = nullptr;
	if (!GetASCAndWeapon(ASC, AscASC, Weapon) || Weapon->IsReloading())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (!Weapon->CanFire(GetAmmoCostPerTrigger(ASC)))
	{
		TryAutoReload(ASC, Weapon);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	RequestShot();
	if (IsActive())
	{
		ScheduleNextTick();
	}
}

void UGA_Fire_Auto::ScheduleNextTick()
{
	const float Interval = GetShotIntervalSeconds(GetAbilitySystemComponentFromActorInfo());
	TickTask = UAbilityTask_WaitDelay::WaitDelay(this, Interval);
	TickTask->OnFinish.AddDynamic(this, &ThisClass::HandleTickFire);
	TickTask->ReadyForActivation();
}

void UGA_Fire_Auto::OnControlReleased()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Fire_Auto::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
	if (TickTask)
	{
		TickTask->EndTask();
		TickTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

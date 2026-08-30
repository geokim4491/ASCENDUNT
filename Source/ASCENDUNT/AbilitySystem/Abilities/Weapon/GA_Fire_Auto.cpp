// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_Fire_Auto.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"

UGA_Fire_Auto::UGA_Fire_Auto()
{
	ActivationPolicy = EAscenduntAbilityActivationPolicy::WhileInputActive;
	ActivationGroup = EAscenduntAbilityActivationGroup::Independent;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_Fire_Auto::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	bStopRequested = true;

	// 딜레이가 걸려있으면 끊고 즉시 종료
	if (TickTask)
	{
		TickTask->EndTask();
		TickTask = nullptr;
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_Fire_Auto::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bStopRequested = false;
	ShotIndex = 0;

	// 즉시 1발 쏘고, 능력은 종료하지 말고 루프 유지
	HandleTickFire();
}

void UGA_Fire_Auto::HandleTickFire()
{
	if (bStopRequested || !CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 1회 트리거 발사 (Base에 만든 FireOnce_Trigger 사용)
	// 실패(탄 부족 등)면 종료
	FAscenduntFireOnceContext Ctx;
	if (!FireOnce_Trigger(/*DamageCoef*/1.f, ShotIndex, Ctx, /*bDoCommit*/true))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// FireType 분기
	const EAscenduntWeaponFireType FireType = Ctx.Weapon->GetCurrentModeStats().FireType;

	bool bOk = false;
	switch (FireType)
	{
	case EAscenduntWeaponFireType::Projectile:
		bOk = FireDelivery_Projectile(Ctx);
		break;
	case EAscenduntWeaponFireType::Hitscan:
	default:
		bOk = FireDelivery_Hitscan(Ctx);
		break;
	}

	if (!bOk)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	++ShotIndex;
	ScheduleNextTick();
}

void UGA_Fire_Auto::ScheduleNextTick()
{
	if (bStopRequested)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// ROF -> interval
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const float Interval = GetShotIntervalSeconds(ASC);

	TickTask = UAbilityTask_WaitDelay::WaitDelay(this, Interval);
	TickTask->OnFinish.AddDynamic(this, &ThisClass::HandleTickFire);
	TickTask->ReadyForActivation();
}


void UGA_Fire_Auto::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	bStopRequested = true;

	if (TickTask)
	{
		TickTask->EndTask();
		TickTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

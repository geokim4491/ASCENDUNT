// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_Fire_Charge.h"

#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"

UGA_Fire_Charge::UGA_Fire_Charge()
{
	ActivationPolicy = EAscenduntAbilityActivationPolicy::WhileInputActive;
	ActivationGroup = EAscenduntAbilityActivationGroup::Independent;
	// Charge는 상태(시작 시간)를 들고 있어야 함
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

double UGA_Fire_Charge::GetWorldTimeSecondsSafe(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo) return 0.0;

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	return World ? (double)World->GetTimeSeconds() : 0.0;
}

float UGA_Fire_Charge::EvalDamageCoef(float HeldTimeSec) const
{
	if (!CachedWeapon)
		CachedWeapon = Cast<AAscenduntWeapon>(GetCurrentSourceObject());

	const float Clamped = FMath::Clamp(HeldTimeSec, 0.f, CachedWeapon->GetCurrentModeStats().MaxChargeTime);
	
	if (IsValid(CachedWeapon->GetCurrentModeStats().ChargeDamageCoefCurve))
	{
		return CachedWeapon->GetCurrentModeStats().ChargeDamageCoefCurve->GetFloatValue(Clamped);
	}

	return 1.f;
}

void UGA_Fire_Charge::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ChargeStartTimeSec = GetWorldTimeSecondsSafe(ActorInfo);
	bDidFire_Server = false;

	// 여기서 Commit 하지 않는다 (Release 때만 발사/코스트)
	// (원하면 여기서 차징 시작 VFX/SFX)
}

void UGA_Fire_Charge::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	//로컬 예측: 연출만(Commit X)
	if (ActorInfo && ActorInfo->IsLocallyControlled() && !ActorInfo->IsNetAuthority())
	{
		FAscenduntFireOnceContext Ctx;
		if (FireOnce_Trigger(/*DamageCoef*/1.f, /*ShotIndex*/0, Ctx, /*bDoCommit*/false))
		{
			const EAscenduntWeaponFireType FireType = Ctx.Weapon->GetCurrentModeStats().FireType;
			if (FireType == EAscenduntWeaponFireType::Projectile)
			{
				FireDelivery_Projectile(Ctx);
			}
			else
			{
				FireDelivery_Hitscan(Ctx);
			}
		}
	}

	// Release는 곧 종료. 서버에도 전파되게 replicate=true로 종료.
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}

void UGA_Fire_Charge::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 서버만 권한 발사
	if (ActorInfo && ActorInfo->IsNetAuthority() && !bWasCancelled && !bDidFire_Server)
	{
		bDidFire_Server = true;

		const float Held = (float)FMath::Max(0.0, GetWorldTimeSecondsSafe(ActorInfo) - ChargeStartTimeSec);
		const float DamageCoef = EvalDamageCoef(Held);

		FAscenduntFireOnceContext Ctx;
		if (FireOnce_Trigger(DamageCoef, /*ShotIndex*/0, Ctx, /*bDoCommit*/true))
		{
			const EAscenduntWeaponFireType FireType = Ctx.Weapon->GetCurrentModeStats().FireType;
			if (FireType == EAscenduntWeaponFireType::Projectile)
			{
				FireDelivery_Projectile(Ctx);
			}
			else
			{
				FireDelivery_Hitscan(Ctx);
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

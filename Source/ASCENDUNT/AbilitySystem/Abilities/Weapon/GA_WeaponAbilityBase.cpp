// Fill out your copyright notice in the Description page of Project Settings.

#include "GA_WeaponAbilityBase.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "UObject/ConstructorHelpers.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/AscenduntGameplayEffectContext.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySourceInterface.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/System/AscCollisionChannels.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"

UGA_WeaponAbilityBase::UGA_WeaponAbilityBase()
{
	TraceChannel = AscCollision::WeaponTrace;

	static ConstructorHelpers::FClassFinder<UGameplayEffect> DamageGEClass(
		TEXT("/Game/AbilitySystem/GE/GE_Damage_Instant")
	);
	static ConstructorHelpers::FClassFinder<UGameplayEffect> HealGEClass(
		TEXT("/Game/AbilitySystem/GE/GE_Heal_Instant")
	);

	if (DamageGEClass.Succeeded())
	{
		DamageEffectClass = DamageGEClass.Class;
	}
	if (HealGEClass.Succeeded())
	{
		HealEffectClass = HealGEClass.Class;
	}
}

bool UGA_WeaponAbilityBase::GetASCAndWeapon(UAbilitySystemComponent*& OutASC, UAscenduntAbilitySystemComponent*& OutAscASC, AAscenduntWeapon*& OutWeapon) const
{
	OutASC = nullptr;
	OutAscASC = nullptr;
	OutWeapon = nullptr;

	if (!CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		return false;
	}

	OutASC = CurrentActorInfo->AbilitySystemComponent.Get();
	OutAscASC = Cast<UAscenduntAbilitySystemComponent>(OutASC);
	OutWeapon = Cast<AAscenduntWeapon>(GetCurrentSourceObject());

	return (OutASC && OutAscASC && OutWeapon);
}

void UGA_WeaponAbilityBase::ApplyImpactDecision_Server(const FAscenduntWeaponAttackContext& Ctx, const FAscShotImpactDecision& Decision, const FVector& TraceStart) const
{
	check(Ctx.bAuthority);

	UAscenduntAbilitySystemComponent* SourceASC = Ctx.AscASC.Get();
	AAscenduntWeapon* ContextWeapon = Ctx.Weapon.Get();
	if (!SourceASC || !ContextWeapon) return;

	UAbilitySystemComponent* TargetASC = Decision.TargetASC.Get();
	if (!TargetASC) return;

	FGameplayEffectContextHandle EffectCtx = SourceASC->MakeEffectContext();
	EffectCtx.AddSourceObject(GetCurrentSourceObject());
	EffectCtx.AddOrigin(TraceStart);
	EffectCtx.AddHitResult(Decision.Hit, true);

	// AbilitySource 세팅 (Execution에서 거리감쇠/헤드샷 등 사용)
	if (FAscenduntGameplayEffectContext* Typed = FAscenduntGameplayEffectContext::ExtractEffectContext(EffectCtx))
	{
		Typed->SetAbilitySource(Cast<IAscenduntAbilitySourceInterface>(GetCurrentSourceObject()), GetAbilityLevel());
	}

	if (Decision.Kind == EAscShotImpactKind::HealTarget)
	{
		if (!HealEffectClass) return;
		FGameplayEffectSpecHandle HealSpec = SourceASC->MakeOutgoingSpec(HealEffectClass, GetAbilityLevel(), EffectCtx);
		if (!HealSpec.IsValid()) return;

		if (ContextWeapon)
		{
			HealSpec.Data->SetSetByCallerMagnitude(
				AscenduntGameplayTags::Data_Weapon_BaseHeal,
				ContextWeapon->GetCurrentModeStats().BaseHeal
			);
		}

		if (!FMath::IsNearlyEqual(Ctx.DamageCoef, 1.f))
		{
			HealSpec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_DamageCoef, Ctx.DamageCoef);
		}

		SourceASC->ApplyGameplayEffectSpecToTarget(*HealSpec.Data.Get(), TargetASC);
		return;
	}

	if (Decision.Kind == EAscShotImpactKind::DamageTarget)
	{
		if (!DamageEffectClass) return;
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), EffectCtx);
		if (!Spec.IsValid()) return;

		if (ContextWeapon)
		{
			Spec.Data->SetSetByCallerMagnitude(
				AscenduntGameplayTags::Data_Weapon_BaseDamage,
				ContextWeapon->GetCurrentModeStats().BaseDamage
			);
		}

		// 근접/특수 공격 전용 절대 데미지 우선
		if (Ctx.DamageOverride >= 0.f)
		{
			Spec.Data->SetSetByCallerMagnitude(
				AscenduntGameplayTags::Data_DamageOverride,
				Ctx.DamageOverride
			);
		}
		else if (!FMath::IsNearlyEqual(Ctx.DamageCoef, 1.f))
		{
			Spec.Data->SetSetByCallerMagnitude(
				AscenduntGameplayTags::Data_DamageCoef,
				Ctx.DamageCoef
			);
		}

		Spec.Data->SetSetByCallerMagnitude(
			AscenduntGameplayTags::Data_Instances,
			FMath::Max(1.f, Ctx.Instances)
		);

		if (Ctx.bAOEImpact)
		{
			Spec.Data->AddDynamicAssetTag(AscenduntGameplayTags::Effect_Weapon_AOEImpact);
		}

		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}

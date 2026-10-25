#include "AscenduntWeaponSet.h"
// Fill out your copyright notice in the Description page of Project Settings.

#include "Net/UnrealNetwork.h"

UAscenduntWeaponSet::UAscenduntWeaponSet() {}

void UAscenduntWeaponSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, CurrentAmmo, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, RateOfFire, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, ShotsPerTrigger, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, AmmoCostPerShot, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, MagSize, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, ReloadTime, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, BaseSpreadDeg, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, SpreadIncPerShot, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, SpreadRecoverPerSec, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, RecoilPitch, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, RecoilYaw, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, HeadshotDamageMultiplier, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, MoveSpeedScalarWhileFiring, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, MoveSpeedScalarWhileADS, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, ProjectileSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, ProjectileScale, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, MeleePlayRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntWeaponSet, MeleeDamageCoef, COND_None, REPNOTIFY_Always);
}

#define ASC_REPNOTIFY(PropName) GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntWeaponSet, PropName, OldValue)

void UAscenduntWeaponSet::OnRep_RateOfFire(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(RateOfFire);
}

void UAscenduntWeaponSet::OnRep_ShotsPerTrigger(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(ShotsPerTrigger);
}

void UAscenduntWeaponSet::OnRep_AmmoCostPerShot(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(AmmoCostPerShot);
}

void UAscenduntWeaponSet::OnRep_MagSize(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(MagSize);
}

void UAscenduntWeaponSet::OnRep_ReloadTime(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(ReloadTime);
}

void UAscenduntWeaponSet::OnRep_BaseSpreadDeg(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(BaseSpreadDeg);
}

void UAscenduntWeaponSet::OnRep_SpreadIncPerShot(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(SpreadIncPerShot);
}

void UAscenduntWeaponSet::OnRep_SpreadRecoverPerSec(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(SpreadRecoverPerSec);
}

void UAscenduntWeaponSet::OnRep_RecoilPitch(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(RecoilPitch);
}

void UAscenduntWeaponSet::OnRep_RecoilYaw(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(RecoilYaw);
}

void UAscenduntWeaponSet::OnRep_HeadshotDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(HeadshotDamageMultiplier);
}

void UAscenduntWeaponSet::OnRep_MoveSpeedScalarWhileFiring(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(MoveSpeedScalarWhileFiring);
}

void UAscenduntWeaponSet::OnRep_MoveSpeedScalarWhileADS(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(MoveSpeedScalarWhileADS);
}

void UAscenduntWeaponSet::OnRep_ProjectileSpeed(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(ProjectileSpeed);
}

void UAscenduntWeaponSet::OnRep_ProjectileScale(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(ProjectileScale);
}

void UAscenduntWeaponSet::OnRep_MeleePlayRate(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(MeleePlayRate);
}

void UAscenduntWeaponSet::OnRep_MeleeDamageCoef(const FGameplayAttributeData& OldValue)
{
	ASC_REPNOTIFY(MeleeDamageCoef)
}

void UAscenduntWeaponSet::OnRep_CurrentAmmo(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntWeaponSet, CurrentAmmo, OldValue);
}

void UAscenduntWeaponSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetCurrentAmmoAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, FMath::Max(0.f, GetMagSize()));
	}
}

void UAscenduntWeaponSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	if (Attribute == GetCurrentAmmoAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, FMath::Max(0.f, GetMagSize()));
	}
}

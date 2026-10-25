// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntCharacterStatSet.h"

#include "Net/UnrealNetwork.h"

UAscenduntCharacterStatSet::UAscenduntCharacterStatSet()
{
	// --- Movement defaults ---
	InitBaseMoveSpeed(600.f);
	InitCrouchMoveSpeedScalar(0.6f);
	InitJumpZVelocity(450.f);
	InitAirControl(0.2f);

	// --- Combat / survival defaults ---
	InitDamageDealtScalar(1.0f);
	InitDamageTakenScalar(1.0f);
	InitHealingReceivedScalar(1.0f);

	// --- Cooldown defaults ---
	InitCooldownDurationScalar(1.0f);

	// --- Sound defaults ---
	InitFootstepSoundScalar(1.0f);
}

void UAscenduntCharacterStatSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntCharacterStatSet, BaseMoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntCharacterStatSet, CrouchMoveSpeedScalar, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntCharacterStatSet, JumpZVelocity, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntCharacterStatSet, AirControl, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntCharacterStatSet, DamageDealtScalar, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntCharacterStatSet, DamageTakenScalar, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntCharacterStatSet, HealingReceivedScalar, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntCharacterStatSet, CooldownDurationScalar, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntCharacterStatSet, FootstepSoundScalar, COND_None, REPNOTIFY_Always);
}

void UAscenduntCharacterStatSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 음수/이상치 방지
	if (Attribute == GetBaseMoveSpeedAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
	else if (Attribute == GetCrouchMoveSpeedScalarAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
	else if (Attribute == GetJumpZVelocityAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
	else if (Attribute == GetAirControlAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
	else if (Attribute == GetDamageDealtScalarAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
	else if (Attribute == GetDamageTakenScalarAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
	else if (Attribute == GetHealingReceivedScalarAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
	else if (Attribute == GetCooldownDurationScalarAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
	else if (Attribute == GetFootstepSoundScalarAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
}

void UAscenduntCharacterStatSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	// Base 값 변경 시에도 동일한 Clamp 보장
	if (Attribute == GetBaseMoveSpeedAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
	else if (Attribute == GetCrouchMoveSpeedScalarAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
	else if (Attribute == GetJumpZVelocityAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
	else if (Attribute == GetAirControlAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
	else if (Attribute == GetDamageDealtScalarAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
	else if (Attribute == GetDamageTakenScalarAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
	else if (Attribute == GetHealingReceivedScalarAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
	else if (Attribute == GetCooldownDurationScalarAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
	else if (Attribute == GetFootstepSoundScalarAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 10.f);
	}
}

void UAscenduntCharacterStatSet::OnRep_BaseMoveSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntCharacterStatSet, BaseMoveSpeed, OldValue);
}

void UAscenduntCharacterStatSet::OnRep_CrouchMoveSpeedScalar(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntCharacterStatSet, CrouchMoveSpeedScalar, OldValue);
}

void UAscenduntCharacterStatSet::OnRep_JumpZVelocity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntCharacterStatSet, JumpZVelocity, OldValue);
}

void UAscenduntCharacterStatSet::OnRep_AirControl(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntCharacterStatSet, AirControl, OldValue);
}

void UAscenduntCharacterStatSet::OnRep_DamageDealtScalar(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntCharacterStatSet, DamageDealtScalar, OldValue);
}

void UAscenduntCharacterStatSet::OnRep_DamageTakenScalar(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntCharacterStatSet, DamageTakenScalar, OldValue);
}

void UAscenduntCharacterStatSet::OnRep_HealingReceivedScalar(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntCharacterStatSet, HealingReceivedScalar, OldValue);
}

void UAscenduntCharacterStatSet::OnRep_CooldownDurationScalar(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntCharacterStatSet, CooldownDurationScalar, OldValue);
}

void UAscenduntCharacterStatSet::OnRep_FootstepSoundScalar(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntCharacterStatSet, FootstepSoundScalar, OldValue);
}

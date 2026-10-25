#include "GA_Fire_Charge.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "TimerManager.h"

UGA_Fire_Charge::UGA_Fire_Charge()
{
	ActivationPolicy = EAscenduntAbilityActivationPolicy::WhileInputActive;
}

void UGA_Fire_Charge::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ChargeStartTimeSec = GetWorld()->GetTimeSeconds();
	ReleasedHeldTime = 0.f;
	bShotConsumed = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

float UGA_Fire_Charge::GetShotDamageCoefficient() const
{
	const AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(GetCurrentSourceObject());
	if (!Weapon)
	{
		return 1.f;
	}

	const auto& Stats = Weapon->GetCurrentModeStats();
	return Stats.ChargeDamageCoefCurve ? Stats.ChargeDamageCoefCurve->GetFloatValue(FMath::Clamp(ReleasedHeldTime, 0.f, Stats.MaxChargeTime)) : 1.f;
}

void UGA_Fire_Charge::OnControlReleased()
{
	ReleasedHeldTime = float(FMath::Max(0., GetWorld()->GetTimeSeconds() - ChargeStartTimeSec));
	if (CurrentActorInfo->IsLocallyControlled())
	{
		RequestShot();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	else
	{
		// The release event precedes the child's activation RPC. Never fire in this callback.
		GetWorld()->GetTimerManager().SetTimer(ReleaseExpiryTimer, this, &ThisClass::ExpireReleasedControl, 2.f, false);
	}
}

void UGA_Fire_Charge::ExpireReleasedControl()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Fire_Charge::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ReleaseExpiryTimer);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

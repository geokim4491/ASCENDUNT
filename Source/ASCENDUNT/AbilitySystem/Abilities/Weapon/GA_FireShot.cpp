#include "GA_FireShot.h"
#include "GA_FireControlBase.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"
#include "ASCENDUNT/Weapon/AscenduntProjectile.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "Engine/World.h"

UGA_FireShot::UGA_FireShot()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	ActivationPolicy = EAscenduntAbilityActivationPolicy::OnInputTriggered;
}

bool UGA_FireShot::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(GetSourceObject(Handle, ActorInfo));
	const UGA_FireControlBase* Control = Weapon ? Weapon->GetActiveFireControl() : nullptr;
	if (!ActorInfo || !Weapon || Weapon->GetOwner() != ActorInfo->AvatarActor.Get() || Weapon->GetCachedAscASC() != ActorInfo->AbilitySystemComponent.Get()
	    || !Control || !Control->MatchesShotClass(GetClass()) || !Control->CanIssueShot())
	{
		return false;
	}

	const UWorld* World = ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld() : nullptr;
	if (!World || !Weapon->CanFireByTime(ActorInfo->IsNetAuthority(), World->GetTimeSeconds()))
	{
		return false;
	}

	const auto& Mode = Weapon->GetCurrentModeStats();
	return Mode.FireType != EAscenduntWeaponFireType::Projectile || Mode.ProjectileClass;
}

void UGA_FireShot::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(GetSourceObject(Handle, ActorInfo));
	UGA_FireControlBase* Control = Weapon ? Weapon->GetActiveFireControl() : nullptr;
	FAscenduntWeaponAttackContext Context;
	if (!Weapon || !Control || !TryPrepareShot(Control->GetShotDamageCoefficient(), 0, Context, false) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Control->ConsumeShotPermission();

	const double PreviousDeadline = Weapon->GetFireDeadline(false);
	if (Weapon->GetCurrentModeStats().FireType == EAscenduntWeaponFireType::Projectile)
	{
		FireProjectile(Context);
	}
	else
	{
		FireHitscan(Context);
	}

	if (ActorInfo->IsLocallyControlled() && !ActorInfo->IsNetAuthority())
	{
		Weapon->RegisterPredictedShot(ActivationInfo.GetActivationPredictionKey(), PreviousDeadline);
	}

	UE_LOG(LogTemp, Verbose, TEXT("[WeaponShot] Role=%d Weapon=%s Key=%d Scope=%d Ammo=%d"), int32(ActorInfo->IsNetAuthority()),
	    *GetNameSafe(Weapon), ActivationInfo.GetActivationPredictionKey().Current, ActorInfo->AbilitySystemComponent->ScopedPredictionKey.Current,
	    Weapon->GetCurrentAmmoInMag());

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}

void UGA_FireShot::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	if (AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(Spec.SourceObject.Get()))
	{
		Weapon->ClearPredictedWeaponState();
	}

	Super::OnRemoveAbility(ActorInfo, Spec);
}

#include "GA_HakukoSkills.h"

#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntUltimateSet.h"
#include "ASCENDUNT/AbilitySystem/Data/AscAbilityData.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Character/Hero/Hakuko/HakukoCharacter.h"
#include "ASCENDUNT/Character/Hero/Hakuko/HakukoGrenadeProjectile.h"
#include "ASCENDUNT/Character/Hero/Hakuko/HakukoSkillComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

namespace AscHakukoAbility
{
	static AHakukoCharacter* GetHakuko(const FGameplayAbilityActorInfo* ActorInfo)
	{
		return ActorInfo ? Cast<AHakukoCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	}

	static bool GetView(const FGameplayAbilityActorInfo* ActorInfo, FVector& OutLocation, FVector& OutForward, FVector& OutRight)
	{
		AHakukoCharacter* Hakuko = GetHakuko(ActorInfo);
		if (!Hakuko) return false;

		FRotator Rot = Hakuko->GetActorRotation();
		OutLocation = Hakuko->GetPawnViewLocation();

		if (ActorInfo->IsNetAuthority())
		{
			// ServerOnly grenade abilities run on Dedicated Server, where there is no local camera.
			if (ActorInfo->PlayerController.IsValid())
			{
				Rot = ActorInfo->PlayerController->GetControlRotation();
			}
		}
		else if (ActorInfo->PlayerController.IsValid())
		{
			ActorInfo->PlayerController->GetPlayerViewPoint(OutLocation, Rot);
		}

		OutForward = Rot.Vector().GetSafeNormal();
		OutRight = FRotationMatrix(Rot).GetUnitAxis(EAxis::Y);
		return true;
	}

	static AHakukoGrenadeProjectile* SpawnGrenade(const FGameplayAbilityActorInfo* ActorInfo, UObject* SourceObject,
		const FHakukoGrenadeProjectileConfig& Config, EHakukoGrenadePayload Payload,
		TSubclassOf<UGameplayEffect> DamageEffect, TSubclassOf<UGameplayEffect> HealEffect,
		float Damage, float Heal, float ExplosionRadius, FGameplayTag CueTag)
	{
		AHakukoCharacter* Hakuko = GetHakuko(ActorInfo);
		UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		if (!Hakuko || !Hakuko->HasAuthority() || !ASC || !Config.ProjectileClass) return nullptr;

		FVector ViewLocation, Forward, Right;
		if (!GetView(ActorInfo, ViewLocation, Forward, Right)) return nullptr;
		const FVector SpawnLocation = Hakuko->GetActorLocation()
			+ Forward * Config.SpawnOffset.X + Right * Config.SpawnOffset.Y + FVector::UpVector * Config.SpawnOffset.Z;
		const FVector Velocity = Forward * Config.ProjectileSpeed + FVector::UpVector * Config.UpwardVelocity;

		FActorSpawnParameters Params;
		Params.Owner = Hakuko;
		Params.Instigator = Hakuko;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AHakukoGrenadeProjectile* Grenade = Hakuko->GetWorld()->SpawnActor<AHakukoGrenadeProjectile>(
			Config.ProjectileClass, SpawnLocation, Velocity.Rotation(), Params);
		if (Grenade)
		{
			Grenade->InitializeGrenade(ASC, SourceObject, Payload, DamageEffect, HealEffect, Damage, Heal,
				ExplosionRadius, Velocity, Config.GravityScale, Config.ProjectileScale, Config.CollisionRadius,
				Config.ActiveDuration, CueTag, Config.bDebugDrawProjectileCollision,
				Config.bDebugDrawExplosionRadius, Config.DebugDrawDuration);
		}
		return Grenade;
	}
}

UGA_HakukoSelectFox::UGA_HakukoSelectFox(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Hakuko_SelectFox);
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}
void UGA_HakukoSelectFox::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}

UGA_HakukoBioGrenade::UGA_HakukoBioGrenade(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Hakuko_BioGrenade);
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}
const UAscAbilityData* UGA_HakukoBioGrenade::GetAbilityData() const { return BioGrenadeData; }
void UGA_HakukoBioGrenade::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!BioGrenadeData || !AscHakukoAbility::GetHakuko(ActorInfo) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return;
	}
	PlayAbilityMontage(BioGrenadeData);

	const float ThrowDelay = FMath::Max(0.0f, BioGrenadeData->ThrowDelay);
	if (ThrowDelay <= KINDA_SMALL_NUMBER)
	{
		ThrowGrenade();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ThrowTimerHandle, this, &ThisClass::ThrowGrenade, ThrowDelay, false);
	}
	else
	{
		FinishDataAbility(true);
	}
}

void UGA_HakukoBioGrenade::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThrowTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_HakukoBioGrenade::ThrowGrenade()
{
	if (!BioGrenadeData || !CurrentActorInfo) { FinishDataAbility(true); return; }
	AHakukoGrenadeProjectile* Grenade = AscHakukoAbility::SpawnGrenade(CurrentActorInfo, this, BioGrenadeData->Projectile,
		EHakukoGrenadePayload::Biotic, BioGrenadeData->DamageEffectClass, BioGrenadeData->HealEffectClass,
		BioGrenadeData->Damage, BioGrenadeData->Heal, BioGrenadeData->ExplosionRadius, BioGrenadeData->GameplayCueTag);
	FinishDataAbility(Grenade == nullptr);
}
UGA_HakukoSendFox::UGA_HakukoSendFox(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Hakuko_SendFox);
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

UGA_HakukoStim::UGA_HakukoStim(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Hakuko_Stim);
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}
const UAscAbilityData* UGA_HakukoStim::GetAbilityData() const { return StimData; }
void UGA_HakukoStim::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	AHakukoCharacter* Hakuko = AscHakukoAbility::GetHakuko(ActorInfo);
	UHakukoSkillComponent* Skill = Hakuko ? Hakuko->GetHakukoSkillComponent() : nullptr;
	AActor* TargetActor = Skill ? Skill->FindBestFriendlyTarget_Server() : nullptr;
	UAbilitySystemComponent* SourceASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UAbilitySystemComponent* TargetASC = TargetActor ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor) : nullptr;
	if (!StimData || !StimData->StimEffectClass || !SourceASC || !TargetASC || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return;
	}

	FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
	Ctx.AddInstigator(Hakuko, Hakuko);
	Ctx.AddSourceObject(this);
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(StimData->StimEffectClass, GetAbilityLevel(), Ctx);
	if (!Spec.IsValid()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	Spec.Data->SetDuration(StimData->Duration, true);
	Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Hakuko_Stim_MoveSpeedMultiplier, StimData->MoveSpeedMultiplier);
	Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Hakuko_Stim_RateOfFireMultiplier, StimData->RateOfFireMultiplier);
	Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Hakuko_Stim_DamageDealtMultiplier, StimData->DamageDealtMultiplier);
	Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Hakuko_Stim_DamageTakenMultiplier, StimData->DamageTakenMultiplier);
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	PlayAbilityMontage(StimData);
	FinishDataAbility(false);
}
UGA_HakukoDash::UGA_HakukoDash(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Hakuko_Dash);
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

UGA_HakukoHealGrenade::UGA_HakukoHealGrenade(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Hakuko_HealGrenade);
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}
const UAscAbilityData* UGA_HakukoHealGrenade::GetAbilityData() const { return HealGrenadeData; }
void UGA_HakukoHealGrenade::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!HealGrenadeData || !AscHakukoAbility::GetHakuko(ActorInfo) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return;
	}
	PlayAbilityMontage(HealGrenadeData);
	FireHealGrenade();
}
void UGA_HakukoHealGrenade::FireHealGrenade()
{
	AHakukoCharacter* Hakuko = AscHakukoAbility::GetHakuko(CurrentActorInfo);
	UHakukoSkillComponent* Skill = Hakuko ? Hakuko->GetHakukoSkillComponent() : nullptr;
	if (!HealGrenadeData || !Skill) { FinishDataAbility(true); return; }
	const float SnapshotHeal = Skill->CalculateHealGrenadeAmount(HealGrenadeData->MinHeal, HealGrenadeData->MaxHeal);
	AHakukoGrenadeProjectile* Grenade = AscHakukoAbility::SpawnGrenade(CurrentActorInfo, this, HealGrenadeData->Projectile,
		EHakukoGrenadePayload::HealOnly, nullptr, HealGrenadeData->HealEffectClass, 0.f, SnapshotHeal,
		HealGrenadeData->ExplosionRadius, HealGrenadeData->GameplayCueTag);
	if (Grenade) Skill->ResetHealGrenadeGauge_Server();
	FinishDataAbility(Grenade == nullptr);
}

UGA_HakukoUltimate::UGA_HakukoUltimate(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Hakuko_Ultimate);
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}
const UAscAbilityData* UGA_HakukoUltimate::GetAbilityData() const { return UltimateData; }
bool UGA_HakukoUltimate::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) || !UltimateData ||
		!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid()) return false;
	const AHakukoCharacter* Hakuko = ActorInfo ? Cast<AHakukoCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Hakuko || !Hakuko->GetHakukoSkillComponent()) return false;
	float ActiveRadius = 0.f;
	float ActiveCoef = 1.f;
	bool bDebugDrawRadius = false;
	float DebugDrawDuration = 0.f;
	if (Hakuko->GetHakukoSkillComponent()->GetUltimateAOEParameters(
		ActiveRadius, ActiveCoef, bDebugDrawRadius, DebugDrawDuration)) return false;

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const float Charge = ASC->GetNumericAttribute(UAscenduntUltimateSet::GetUltimateChargeAttribute());
	const float MaxCharge = ASC->GetNumericAttribute(UAscenduntUltimateSet::GetMaxUltimateChargeAttribute());
	return MaxCharge > KINDA_SMALL_NUMBER && Charge + KINDA_SMALL_NUMBER >= MaxCharge;
}
void UGA_HakukoUltimate::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	AHakukoCharacter* Hakuko = AscHakukoAbility::GetHakuko(ActorInfo);
	UHakukoSkillComponent* Skill = Hakuko ? Hakuko->GetHakukoSkillComponent() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const float Charge = ASC ? ASC->GetNumericAttribute(UAscenduntUltimateSet::GetUltimateChargeAttribute()) : 0.f;
	const float MaxCharge = ASC ? ASC->GetNumericAttribute(UAscenduntUltimateSet::GetMaxUltimateChargeAttribute()) : 0.f;
	if (!UltimateData || !Skill || !ASC || MaxCharge <= KINDA_SMALL_NUMBER || Charge + KINDA_SMALL_NUMBER < MaxCharge)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return;
	}

	ASC->SetNumericAttributeBase(UAscenduntUltimateSet::GetUltimateChargeAttribute(), 0.f);
	if (UltimateData->BuffEffectClass)
	{
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UltimateData->BuffEffectClass, GetAbilityLevel(), ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetDuration(UltimateData->Duration, true);
			Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Hakuko_Ultimate_DamageDealtMultiplier, UltimateData->DamageDealtMultiplier);
			Spec.Data->DynamicGrantedTags.AddTag(AscenduntGameplayTags::Status_Hakuko_Ultimate);
			Spec.Data->DynamicGrantedTags.AddTag(AscenduntGameplayTags::Status_Ultimate_Active);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	Skill->ActivateUltimate_Server(
		UltimateData->Duration,
		UltimateData->AOERadius,
		UltimateData->AOEDamageCoefficient,
		UltimateData->WeaponVFX,
		UltimateData->bDebugDrawAOERadius,
		UltimateData->AOEDebugDrawDuration);
	PlayAbilityMontage(UltimateData);
	FinishDataAbility(false);
}

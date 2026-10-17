#include "GA_ADS.h"

#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Camera/AscenduntCameraComponent.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/HeroFirstPersonComponent.h"
#include "ASCENDUNT/Character/Hero/HeroInputComponent.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"

UGA_ADS::UGA_ADS()
{
	ActivationPolicy = EAscenduntAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EAscenduntAbilityActivationGroup::Independent;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// FOV / sensitivity / 1P pose are purely owning-client presentation.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;

	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Weapon_ADS);
	ActivationOwnedTags.AddTag(AscenduntGameplayTags::Status_Weapon_ADS);
}

bool UGA_ADS::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const APawn* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return false;
	}

	const AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(GetSourceObject(Handle, ActorInfo));
	return Weapon != nullptr;
}

void UGA_ADS::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(GetSourceObject(Handle, ActorInfo));
	if (!Weapon)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	ApplyLocalADSPresentation(true, Weapon, ActorInfo);
	bPresentationApplied = true;
}

void UGA_ADS::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}

void UGA_ADS::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	// WeaponAbilitySet can disappear because of weapon swap/hero teardown while RMB is still held.
	// Make sure local presentation can never remain stuck in ADS after the spec is removed.
	if (bPresentationApplied)
	{
		ApplyLocalADSPresentation(false, nullptr, ActorInfo);
		bPresentationApplied = false;
	}

	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UGA_ADS::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (bPresentationApplied)
	{
		ApplyLocalADSPresentation(false, nullptr, ActorInfo);
		bPresentationApplied = false;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ADS::ApplyLocalADSPresentation(
	bool bEnabled,
	AAscenduntWeapon* Weapon,
	const FGameplayAbilityActorInfo* ActorInfo)
{
	const FGameplayAbilityActorInfo* UseActorInfo = ActorInfo ? ActorInfo : CurrentActorInfo;
	AHeroCharacter* Hero = UseActorInfo ? Cast<AHeroCharacter>(UseActorInfo->AvatarActor.Get()) : nullptr;
	if (!Hero || !Hero->IsLocallyControlled())
	{
		return;
	}

	float TargetFOV = 70.0f;
	if (bEnabled && Weapon)
	{
		const FAscenduntWeaponModeStats& Stats = Weapon->GetCurrentModeStats();
		TargetFOV = Stats.ADS_FOV;
	}

	if (UAscenduntCameraComponent* Camera = UAscenduntCameraComponent::FindCameraComponent(Hero))
	{
		Camera->SetADSFieldOfView(bEnabled, TargetFOV);
	}

	if (UHeroInputComponent* HeroInput = Hero->GetHeroInputComponent())
	{
		HeroInput->SetADSActive(bEnabled);
	}

	if (UHeroFirstPersonComponent* FirstPerson = Hero->GetFirstPersonComponent())
	{
		FirstPerson->SetADSActive(bEnabled);
	}
}

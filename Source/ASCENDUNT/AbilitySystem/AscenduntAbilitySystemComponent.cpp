#include "AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/AbilitySystem/Effects/AscWeaponAmmoCost.h"
// Fill out your copyright notice in the Description page of Project Settings.



#include "ASCENDUNT/AbilitySystem/Abilities/Common/GA_DataAbility.h"
#include "ASCENDUNT/AbilitySystem/Components/AscSkillStockComponent.h"
#include "ASCENDUNT/AbilitySystem/Data/AscAbilityData.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "ASCENDUNT/AbilitySystem/Abilities/AscenduntGameplayAbility.h"
#include "ASCENDUNT/AbilitySystem/AbilityTagRelationshipMapping.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Gameplay_AbilityInputBlocked, "Gameplay.AbilityInputBlocked");

UAscenduntAbilitySystemComponent::UAscenduntAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationGroupAbilities.SetNum((int32)EAscenduntAbilityActivationGroup::MAX);
}

void UAscenduntAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UAscenduntAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag)))
			{
				InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.AddUnique(AbilitySpec.Handle);
			}
		}
	}
}

void UAscenduntAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag)))
			{
				InputReleasedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.Remove(AbilitySpec.Handle);
			}
		}
	}
}

void UAscenduntAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	if (HasMatchingGameplayTag(TAG_Gameplay_AbilityInputBlocked))
	{
		ClearAbilityInput();
		return;
	}

	static TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;
	AbilitiesToActivate.Reset();

	//@TODO: See if we can use FScopedServerAbilityRPCBatcher ScopedRPCBatcher in some of these loops

	//
	// Process all abilities that activate when the input is held.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputHeldSpecHandles)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability && !AbilitySpec->IsActive())
			{
				const UAscenduntGameplayAbility* AscenduntAbilityCDO = Cast<UAscenduntGameplayAbility>(AbilitySpec->Ability);
				if (AscenduntAbilityCDO && AscenduntAbilityCDO->GetActivationPolicy() == EAscenduntAbilityActivationPolicy::WhileInputActive)
				{
					AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
				}
			}
		}
	}

	//
	// Process all abilities that had their input pressed this frame.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputPressedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpec->InputPressed = true;

				if (AbilitySpec->IsActive())
				{
					// Ability is active so pass along the input event.
					AbilitySpecInputPressed(*AbilitySpec);
				}
				else
				{
					const UAscenduntGameplayAbility* AscenduntAbilityCDO = Cast<UAscenduntGameplayAbility>(AbilitySpec->Ability);

					if (AscenduntAbilityCDO && AscenduntAbilityCDO->GetActivationPolicy() == EAscenduntAbilityActivationPolicy::OnInputTriggered)
					{
						AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
					}
				}
			}
		}
	}

	// A Hold interaction (revive included) is cancelled when the player tries to
	// activate any other GameplayAbility. Damage does not touch this path, so taking
	// damage does not cancel the interaction. Movement is separately locked by GA_Interact.
	if (HasMatchingGameplayTag(AscenduntGameplayTags::Status_Interacting))
	{
		bool bRequestedOtherAbility = false;
		for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitiesToActivate)
		{
			const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(AbilitySpecHandle);
			if (AbilitySpec && !AbilitySpec->DynamicAbilityTags.HasTagExact(AscenduntGameplayTags::InputTag_Interact))
			{
				bRequestedOtherAbility = true;
				break;
			}
		}

		if (bRequestedOtherAbility)
		{
			FGameplayTagContainer InteractAbilityTags;
			InteractAbilityTags.AddTag(AscenduntGameplayTags::Ability_Interact);
			CancelAbilities(&InteractAbilityTags);
		}
	}

	//
	// Try to activate all the abilities that are from presses and holds.
	// We do it all at once so that held inputs don't activate the ability
	// and then also send a input event to the ability because of the press.
	//
	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitiesToActivate)
	{
		TryActivateAbility(AbilitySpecHandle);
	}

	//
	// Process all abilities that had their input released this frame.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputReleasedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpec->InputPressed = false;

				if (AbilitySpec->IsActive())
				{
					// Ability is active so pass along the input event.
					AbilitySpecInputReleased(*AbilitySpec);
				}
			}
		}
	}

	//
	// Clear the cached ability handles.
	//
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void UAscenduntAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

void UAscenduntAbilitySystemComponent::TryActivateAbilitiesOnSpawn()
{
	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		const UAscenduntGameplayAbility* AscenduntAbility =
			Cast<UAscenduntGameplayAbility>(AbilitySpec.Ability);
		if (AscenduntAbility)
		{
			AscenduntAbility->TryActivateAbilityOnSpawn(ActorInfo, AbilitySpec);
		}
	}
}

void UAscenduntAbilitySystemComponent::SetTagRelationshipMapping(UAbilityTagRelationshipMapping* NewMapping)
{
	TagRelationshipMapping = NewMapping;
}

void UAscenduntAbilitySystemComponent::GetAdditionalActivationTagRequirements(
	const FGameplayTagContainer& AbilityTags,
	FGameplayTagContainer& OutActivationRequired,
	FGameplayTagContainer& OutActivationBlocked) const
{
	if (TagRelationshipMapping)
	{
		TagRelationshipMapping->GetRequiredAndBlockedActivationTags(
			AbilityTags,
			&OutActivationRequired,
			&OutActivationBlocked);
	}
}

void UAscenduntAbilitySystemComponent::ApplyAbilityBlockAndCancelTags(
	const FGameplayTagContainer& AbilityTags,
	UGameplayAbility* RequestingAbility,
	bool bEnableBlockTags,
	const FGameplayTagContainer& BlockTags,
	bool bExecuteCancelTags,
	const FGameplayTagContainer& CancelTags)
{
	FGameplayTagContainer ModifiedBlockTags = BlockTags;
	FGameplayTagContainer ModifiedCancelTags = CancelTags;

	if (TagRelationshipMapping)
	{
		TagRelationshipMapping->GetAbilityTagsToBlockAndCancel(
			AbilityTags,
			&ModifiedBlockTags,
			&ModifiedCancelTags);
	}

	// ServerOnly abilities never run a local ability instance on the owning client, so GAS's
	// normal local block/cancel bookkeeping cannot affect a predicted ability there. Mirror the
	// resolved relationship state explicitly. LocalPredicted abilities are deliberately excluded
	// to avoid applying their block counts twice.
	if (RequestingAbility && IsOwnerActorAuthoritative() &&
		RequestingAbility->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly &&
		((bEnableBlockTags && !ModifiedBlockTags.IsEmpty()) ||
		 (!bEnableBlockTags && !ModifiedBlockTags.IsEmpty()) ||
		 (bExecuteCancelTags && !ModifiedCancelTags.IsEmpty())))
	{
		Client_ApplyServerOnlyAbilityTagRelationshipState(
			ModifiedBlockTags,
			bEnableBlockTags,
			ModifiedCancelTags,
			bExecuteCancelTags);
	}

	Super::ApplyAbilityBlockAndCancelTags(
		AbilityTags,
		RequestingAbility,
		bEnableBlockTags,
		ModifiedBlockTags,
		bExecuteCancelTags,
		ModifiedCancelTags);
}

void UAscenduntAbilitySystemComponent::Client_ApplyServerOnlyAbilityTagRelationshipState_Implementation(
	FGameplayTagContainer BlockTags,
	bool bEnableBlockTags,
	FGameplayTagContainer CancelTags,
	bool bExecuteCancelTags)
{
	// A listen-server host uses the authoritative ASC locally; applying the RPC there would
	// duplicate the block count already maintained by Super::ApplyAbilityBlockAndCancelTags.
	if (IsOwnerActorAuthoritative())
	{
		return;
	}

	if (!BlockTags.IsEmpty())
	{
		if (bEnableBlockTags)
		{
			BlockAbilitiesWithTags(BlockTags);
		}
		else
		{
			UnBlockAbilitiesWithTags(BlockTags);
		}
	}

	if (bExecuteCancelTags && !CancelTags.IsEmpty())
	{
		CancelAbilities(&CancelTags);
	}
}

bool UAscenduntAbilitySystemComponent::GetAbilityCooldownByAbilityTag(FGameplayTag AbilityTag, float& TimeRemaining, float& CooldownDuration) const
{
	TimeRemaining = 0.0f;
	CooldownDuration = 0.0f;

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!AbilityTag.IsValid() || !ActorInfo)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.Ability || !AbilitySpec.Ability->GetAssetTags().HasTagExact(AbilityTag))
		{
			continue;
		}

		AbilitySpec.Ability->GetCooldownTimeRemainingAndDuration(
			AbilitySpec.Handle, ActorInfo, TimeRemaining, CooldownDuration);
		return TimeRemaining > KINDA_SMALL_NUMBER;
	}

	return false;
}

bool UAscenduntAbilitySystemComponent::GetAbilityCooldownByInputTag(FGameplayTag InputTag, float& TimeRemaining, float& CooldownDuration) const
{
	TimeRemaining = 0.0f;
	CooldownDuration = 0.0f;

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!InputTag.IsValid() || !ActorInfo)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.Ability || !AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag))
		{
			continue;
		}

		AbilitySpec.Ability->GetCooldownTimeRemainingAndDuration(
			AbilitySpec.Handle, ActorInfo, TimeRemaining, CooldownDuration);
		return TimeRemaining > KINDA_SMALL_NUMBER;
	}

	return false;
}

bool UAscenduntAbilitySystemComponent::GetAbilityUIStateByInputTag(FGameplayTag InputTag, FAscAbilityUIState& OutState) const
{
	OutState = FAscAbilityUIState();

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!InputTag.IsValid() || !ActorInfo)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.Ability || !AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag))
		{
			continue;
		}

		if (const UGA_DataAbility* DataAbility = Cast<UGA_DataAbility>(AbilitySpec.Ability))
		{
			if (const UAscAbilityData* AbilityData = DataAbility->GetAbilityDataForUI())
			{
				OutState.AbilityIcon = AbilityData->AbilityIcon;
			}

			FGameplayTag StockTag;
			int32 ConfiguredMaxStock = 0;
			float ConfiguredRecoverDuration = 0.0f;
			if (DataAbility->GetSkillStockDefinition(StockTag, ConfiguredMaxStock, ConfiguredRecoverDuration))
			{
				OutState.bUsesStock = true;
				OutState.MaxStock = ConfiguredMaxStock;
				OutState.CurrentStock = ConfiguredMaxStock;
				OutState.Duration = ConfiguredRecoverDuration;

				AActor* AvaActor = ActorInfo->AvatarActor.Get();
				const UAscSkillStockComponent* StockComp = AvaActor
					? AvaActor->FindComponentByClass<UAscSkillStockComponent>()
					: nullptr;

				if (StockComp)
				{
					const int32 ReplicatedMaxStock = StockComp->GetMaxStock(StockTag);
					if (ReplicatedMaxStock > 0)
					{
						OutState.MaxStock = ReplicatedMaxStock;
						OutState.CurrentStock = StockComp->GetCurrentStock(StockTag);
					}

					float RechargeRemaining = 0.0f;
					float RechargeDuration = 0.0f;
					if (StockComp->GetRecoverState(StockTag, RechargeRemaining, RechargeDuration))
					{
						OutState.bShowCooldownOverlay = true;
						OutState.TimeRemaining = RechargeRemaining;
						OutState.Duration = RechargeDuration;
						OutState.OverlayPercent = RechargeDuration > KINDA_SMALL_NUMBER
							? FMath::Clamp(RechargeRemaining / RechargeDuration, 0.0f, 1.0f)
							: 0.0f;
					}
				}

				return true;
			}
		}

		AbilitySpec.Ability->GetCooldownTimeRemainingAndDuration(
			AbilitySpec.Handle, ActorInfo, OutState.TimeRemaining, OutState.Duration);
		OutState.bShowCooldownOverlay = OutState.TimeRemaining > KINDA_SMALL_NUMBER;
		OutState.OverlayPercent = OutState.Duration > KINDA_SMALL_NUMBER
			? FMath::Clamp(OutState.TimeRemaining / OutState.Duration, 0.0f, 1.0f)
			: 0.0f;
		return true;
	}

	return false;
}

bool UAscenduntAbilitySystemComponent::IsActivationGroupBlocked(EAscenduntAbilityActivationGroup Group) const
{
	if (Group == EAscenduntAbilityActivationGroup::Independent)
	{
		return false;
	}

	// Blocking이 하나라도 있으면 다른 Exclusive는 막힘
	const bool bHasBlocking =
		ActivationGroupAbilities[(int32)EAscenduntAbilityActivationGroup::Exclusive_Blocking].Num() > 0;

	return bHasBlocking;
}

void UAscenduntAbilitySystemComponent::AddAbilityToActivationGroup(EAscenduntAbilityActivationGroup Group, UAscenduntGameplayAbility* Ability)
{
	if (!Ability) return;

	auto& List = ActivationGroupAbilities[(int32)Group];
	for (const auto& W : List)
	{
		if (W.Get() == Ability) return; // 중복 방지
	}

	// Blocking이 들어오면 Replaceable을 밀어내기
	if (Group == EAscenduntAbilityActivationGroup::Exclusive_Blocking)
	{
		CancelActivationGroupAbilities(EAscenduntAbilityActivationGroup::Exclusive_Replaceable, Ability);
	}

	List.Add(Ability);
}

void UAscenduntAbilitySystemComponent::RemoveAbilityFromActivationGroup(EAscenduntAbilityActivationGroup Group, UAscenduntGameplayAbility* Ability)
{
	auto& List = ActivationGroupAbilities[(int32)Group];
	List.RemoveAll([&](const TWeakObjectPtr<UAscenduntGameplayAbility>& W)
		{
			return !W.IsValid() || (W.Get() == Ability);
		});
}

void UAscenduntAbilitySystemComponent::CancelActivationGroupAbilities(EAscenduntAbilityActivationGroup Group, UAscenduntGameplayAbility* IgnoreAbility)
{
	auto& List = ActivationGroupAbilities[(int32)Group];

	for (const TWeakObjectPtr<UAscenduntGameplayAbility>& W : List)
	{
		UAscenduntGameplayAbility* Ability = W.Get();
		if (!Ability || Ability == IgnoreAbility) continue;

		// UAbilitySystemComponent의 CancelAbility(UGameplayAbility*) 사용 가능
		CancelAbility(Ability);
	}
}


// =================== Owner-only combat feedback ===================

void UAscenduntAbilitySystemComponent::Client_PlayCombatFeedback_Implementation(
	AActor* FeedbackTarget, FGameplayTag CueTag, FGameplayCueParameters Params)
{
	if (!IsValid(FeedbackTarget) || !CueTag.IsValid())
	{
		return;
	}

	// The RPC is carried by the attacker's ASC, while the cue is executed on its actual target.
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(
		FeedbackTarget, CueTag, EGameplayCueEvent::Executed, Params);
}

void UAscenduntAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	AActor* PreviousAvatar = GetAvatarActor();
	if (PreviousAvatar && PreviousAvatar != InAvatarActor)
	{
		TArray<AAscenduntWeapon*> PreviousWeapons;
		TArray<FGameplayAbilitySpecHandle> PreviousAbilities;
		{
			FScopedAbilityListLock AbilityListLock(*this);
			for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
			{
				if (AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(Spec.SourceObject.Get()); Weapon && Weapon->GetOwner() == PreviousAvatar)
				{
					PreviousWeapons.AddUnique(Weapon);
					if (Spec.IsActive())
					{
						PreviousAbilities.Add(Spec.Handle);
					}
				}
			}
		}

		for (FGameplayAbilitySpecHandle Handle : PreviousAbilities)
		{
			CancelAbilityHandle(Handle);
		}

		for (AAscenduntWeapon* Weapon : PreviousWeapons)
		{
			Weapon->ClearPredictedWeaponState(this);
			Weapon->UnbindAmmoAttributes();
		}
	}

	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
}

void UAscenduntAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
	{
		if (Instance && Instance->IsActive())
		{
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}
}

void UAscenduntAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
	{
		if (Instance && Instance->IsActive())
		{
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle,
			    Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}
}

void UAscenduntAbilitySystemComponent::RemovePredictedWeaponAmmoCost(FActiveGameplayEffectHandle Handle, const AAscenduntWeapon* Weapon)
{
	const FActiveGameplayEffect* Effect = GetActiveGameplayEffect(Handle);
	if (!IsOwnerActorAuthoritative() && Effect && Effect->PredictionKey.IsLocalClientKey() && Effect->Spec.Def && Effect->Spec.Def->IsA<UAscWeaponAmmoCost>()
	    && Effect->Spec.GetContext().GetSourceObject() == Weapon)
	{
		RemoveActiveGameplayEffect_AllowClientRemoval(Handle);
	}
}

void UAscenduntAbilitySystemComponent::Client_WeaponReloadFinished_Implementation(AAscenduntWeapon* Weapon, int16 PredictionKey)
{
	if (Weapon)
	{
		Weapon->FinishPredictedReload(PredictionKey);
	}
}

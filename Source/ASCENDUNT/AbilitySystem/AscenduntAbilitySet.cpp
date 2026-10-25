// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntAbilitySet.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Abilities/AscenduntGameplayAbility.h"
#include "Abilities/GameplayAbility.h"

void FAscenduntAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FAscenduntAbilitySet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FAscenduntAbilitySet_GrantedHandles::AddAttributeSet(UAttributeSet* Set)
{
	if (IsValid(Set))
	{
		GrantedAttributeSets.Add(Set);
	}
}


void FAscenduntAbilitySet_GrantedHandles::TakeFromAbilitySystem(UAscenduntAbilitySystemComponent* AscenduntASC)
{
	check(AscenduntASC);

	if(!AscenduntASC->IsOwnerActorAuthoritative())
	{
		// Must be authoritative to give or take ability sets.
		return;
	}

	// Cooldown GameplayEffects are created by ability activation and are not part of
	// GameplayEffectHandles. Gather their granted cooldown tags before the specs are
	// removed, then explicitly remove those effects. This prevents old-hero or old-weapon
	// cooldowns from leaking after their AbilitySet is taken away.
	FGameplayTagContainer CooldownTagsToRemove;
	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (!Handle.IsValid())
		{
			continue;
		}

		if (const FGameplayAbilitySpec* AbilitySpec = AscenduntASC->FindAbilitySpecFromHandle(Handle))
		{
			if (AbilitySpec->Ability)
			{
				if (const FGameplayTagContainer* CooldownTags = AbilitySpec->Ability->GetCooldownTags())
				{
					CooldownTagsToRemove.AppendTags(*CooldownTags);
				}
			}
		}

		AscenduntASC->ClearAbility(Handle);
	}

	if (!CooldownTagsToRemove.IsEmpty())
	{
		AscenduntASC->RemoveActiveEffectsWithGrantedTags(CooldownTagsToRemove);
	}

	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			AscenduntASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	// Only legacy/AI Grant calls populate this array. Player and weapon grants use
	// Skip, so their persistent PlayerState AttributeSets are never removed.
	for (UAttributeSet* Set : GrantedAttributeSets)
	{
		if (IsValid(Set))
		{
			AscenduntASC->RemoveSpawnedAttribute(Set);
		}
	}

	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
	GrantedAttributeSets.Reset();
}

UAscenduntAbilitySet::UAscenduntAbilitySet(const FObjectInitializer& ObjectInitializer)
	: UPrimaryDataAsset(ObjectInitializer)
{
}

void UAscenduntAbilitySet::GiveToAbilitySystem(UAscenduntAbilitySystemComponent* AscenduntASC, FAscenduntAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject, EAscAttributeSetGrantPolicy AttributeSetPolicy) const
{
	check(AscenduntASC);

	if(!AscenduntASC->IsOwnerActorAuthoritative())
	{
		// Must be authoritative to give or take ability sets.
		return;
	}

	if (AttributeSetPolicy == EAscAttributeSetGrantPolicy::Grant)
	{
		for (int32 SetIndex = 0; SetIndex < GrantedAttributes.Num(); ++SetIndex)
		{
			const FAscenduntAbilitySet_AttributeSet& SetToGrant = GrantedAttributes[SetIndex];

			if (!IsValid(SetToGrant.AttributeSet))
			{
				UE_LOG(LogTemp, Error, TEXT("GrantedAttributes[%d] on ability set [%s] is not valid"), SetIndex, *GetNameSafe(this));
				continue;
			}

			// Native Sets belong to the ASC owner's lifetime, not this removable grant.
			if (const UAttributeSet* ExistingSet = AscenduntASC->GetAttributeSet(SetToGrant.AttributeSet))
			{
				if (ExistingSet->HasAnyFlags(RF_DefaultSubObject))
				{
					continue;
				}
			}

			UAttributeSet* NewSet = NewObject<UAttributeSet>(AscenduntASC->GetOwner(), SetToGrant.AttributeSet);
			AscenduntASC->AddAttributeSetSubobject(NewSet);

			if (OutGrantedHandles)
			{
				OutGrantedHandles->AddAttributeSet(NewSet);
			}
		}
	}

	// Grant the gameplay abilities.
	for (int32 AbilityIndex = 0; AbilityIndex < GrantedGameplayAbilities.Num(); ++AbilityIndex)
	{
		const FAscenduntAbilitySet_GameplayAbility& AbilityToGrant = GrantedGameplayAbilities[AbilityIndex];

		if (!IsValid(AbilityToGrant.Ability))
		{
			UE_LOG(LogTemp, Error, TEXT("GrantedGameplayAbilities[%d] on ability set [%s] is not valid."), AbilityIndex, *GetNameSafe(this));
			continue;
		}

		UAscenduntGameplayAbility* AbilityCDO = AbilityToGrant.Ability->GetDefaultObject<UAscenduntGameplayAbility>();

		FGameplayAbilitySpec AbilitySpec(AbilityCDO, AbilityToGrant.AbilityLevel);
		AbilitySpec.SourceObject = SourceObject;
		AbilitySpec.DynamicAbilityTags.AddTag(AbilityToGrant.InputTag);

		const FGameplayAbilitySpecHandle AbilitySpecHandle = AscenduntASC->GiveAbility(AbilitySpec);

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAbilitySpecHandle(AbilitySpecHandle);
		}
	}

	// Grant the gameplay effects.
	for (int32 EffectIndex = 0; EffectIndex < GrantedGameplayEffects.Num(); ++EffectIndex)
	{
		const FAscenduntAbilitySet_GameplayEffect& EffectToGrant = GrantedGameplayEffects[EffectIndex];

		if (!IsValid(EffectToGrant.GameplayEffect))
		{
			UE_LOG(LogTemp, Error, TEXT("GrantedGameplayEffects[%d] on ability set [%s] is not valid"), EffectIndex, *GetNameSafe(this));
			continue;
		}

		const UGameplayEffect* GameplayEffect = EffectToGrant.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle GameplayEffectHandle = AscenduntASC->ApplyGameplayEffectToSelf(GameplayEffect, EffectToGrant.EffectLevel, AscenduntASC->MakeEffectContext());

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddGameplayEffectHandle(GameplayEffectHandle);
		}
	}
}

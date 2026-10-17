// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/DataAsset.h"

#include "ActiveGameplayEffectHandle.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"

#include "AscenduntAbilitySet.generated.h"

class UAscenduntGameplayAbility;
class UAscenduntAbilitySystemComponent;
class UAttributeSet;
class UGameplayEffect;
class UObject;

/** Controls whether an AbilitySet creates/removes its configured AttributeSets.
 *  AI keeps the legacy Grant behavior. Player/weapon paths use Skip because their
 *  common Sets are persistent C++ subobjects owned by PlayerState.
 */
enum class EAscAttributeSetGrantPolicy : uint8
{
	Grant,
	Skip
};

/**
 * FAscenduntAbilitySet_GameplayAbility
 *
 *	Data used by the ability set to grant gameplay abilities.
 */
USTRUCT(BlueprintType)
struct FAscenduntAbilitySet_GameplayAbility
{
	GENERATED_BODY()

public:

	// Gameplay ability to grant.
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAscenduntGameplayAbility> Ability = nullptr;

	// Level of ability to grant.
	UPROPERTY(EditDefaultsOnly)
	int32 AbilityLevel = 1;

	// Tag used to process input for the ability.
	UPROPERTY(EditDefaultsOnly, Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * FAscenduntAbilitySet_GameplayEffect
 *
 *	Data used by the ability set to grant gameplay effects.
 */
USTRUCT(BlueprintType)
struct FAscenduntAbilitySet_GameplayEffect
{
	GENERATED_BODY()

public:

	// Gameplay effect to grant.
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;

	// Level of gameplay effect to grant.
	UPROPERTY(EditDefaultsOnly)
	float EffectLevel = 1.0f;
};


/**
 * FAscenduntAbilitySet_AttributeSet
 *
 *	Data used by the ability set to grant attribute sets.
 */
USTRUCT(BlueprintType)
struct FAscenduntAbilitySet_AttributeSet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAttributeSet> AttributeSet;
};

/**
 * FAscenduntAbilitySet_GrantedHandles
 *
 *	Data used to store handles to what has been granted by the ability set.
 */
USTRUCT(BlueprintType)
struct FAscenduntAbilitySet_GrantedHandles
{
	GENERATED_BODY()

public:

	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void AddAttributeSet(UAttributeSet* Set);

	void TakeFromAbilitySystem(UAscenduntAbilitySystemComponent* AscenduntASC);

	bool IsEmpty() const
	{
		return AbilitySpecHandles.IsEmpty() &&
			GameplayEffectHandles.IsEmpty() &&
			GrantedAttributeSets.IsEmpty();
	}

	template <class T>
	T* FindGrantedAttributeSet() const
	{
		for (UAttributeSet* Set : GrantedAttributeSets)
		{
			if (T* TypedSet = Cast<T>(Set))
			{
				return TypedSet;
			}
		}
		return nullptr;
	}

protected:

	// Handles to the granted abilities.
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	// Handles to the granted gameplay effects.
	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	// AttributeSets created by this grant. Player/weapon Skip grants leave this empty.
	UPROPERTY()
	TArray<TObjectPtr<UAttributeSet>> GrantedAttributeSets;
};

/**
 * UAscenduntAbilitySet
 * 
 * Non-mutable data asset used to grant gameplay abilities, gameplay effects, and
 * optionally AttributeSets. AI uses the legacy AttributeSet grant path; player and
 * weapon grants skip it because their common Sets are persistent PlayerState subobjects.
 */
UCLASS()
class ASCENDUNT_API UAscenduntAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:

	UAscenduntAbilitySet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Grants the ability set to the specified ability system component.
	// The returned handles can be used later to take away anything that was granted.
	void GiveToAbilitySystem(UAscenduntAbilitySystemComponent* AscenduntASC,
		FAscenduntAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr,
		EAscAttributeSetGrantPolicy AttributeSetPolicy = EAscAttributeSetGrantPolicy::Grant) const;
	
protected:

	// Gameplay abilities to grant when this ability set is granted.
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Abilities", meta = (TitleProperty = Ability))
	TArray<FAscenduntAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	// Gameplay effects to grant when this ability set is granted.
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects", meta = (TitleProperty = GameplayEffect))
	TArray<FAscenduntAbilitySet_GameplayEffect> GrantedGameplayEffects;

	// Used by legacy/AI grants. Player and weapon paths explicitly skip these entries.
	UPROPERTY(EditDefaultsOnly, Category = "Attribute Sets", meta = (TitleProperty = AttributeSet))
	TArray<FAscenduntAbilitySet_AttributeSet> GrantedAttributes;
};

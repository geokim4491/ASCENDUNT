// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySourceInterface.h"
#include "AscenduntGameplayAbility.generated.h"

class UAscenduntAbilitySystemComponent;
class APlayerController;
class AController;
class APawn;
class ACharacter;
class UAscenduntCameraMode;
class UHeroCameraComponent;

/**
 * EAscenduntActivationPolicy
 *
 *	Defines how an ability is meant to activate.
 */
UENUM(BlueprintType)
enum class EAscenduntAbilityActivationPolicy : uint8
{
	// Try to activate the ability when the input is triggered.
	OnInputTriggered,

	// Continually try to activate the ability while the input is active.
	WhileInputActive,

	// Try to activate the ability when an avatar is assigned.
	OnSpawn
};

/**
 * EAscenduntAbilityActivationGroup
 *
 *	Defines how an ability activates in relation to other abilities.
 */
UENUM(BlueprintType)
enum class EAscenduntAbilityActivationGroup : uint8
{
	// Ability runs independently of all other abilities.
	Independent,

	// Ability is canceled and replaced by other exclusive abilities.
	Exclusive_Replaceable,

	// Ability blocks all other exclusive abilities from activating.
	Exclusive_Blocking,

	MAX	UMETA(Hidden)
};

/**
 * UAscenduntGameplayAbility
 * 
 * The base gameplay ability class used by this project.
 */
UCLASS()
class ASCENDUNT_API UAscenduntGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	friend class UAscenduntAbilitySystemComponent;

public:
	UAscenduntGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// --- ActorInfo helpers ---
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Ability")
	UAscenduntAbilitySystemComponent* GetAscenduntASCFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Ability")
	APlayerController* GetPlayerControllerFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Ability")
	AController* GetControllerFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Ability")
	APawn* GetPawnFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Ability")
	ACharacter* GetCharacterFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Ability")
	UHeroCameraComponent* GetHeroCameraComponentFromActorInfo() const;

	// Activation policy/group
	EAscenduntAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }
	EAscenduntAbilityActivationGroup GetActivationGroup() const { return ActivationGroup; }

	void TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const;

	// Returns true if the requested activation group is a valid transition.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Ascendunt|Ability", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool CanChangeActivationGroup(EAscenduntAbilityActivationGroup NewGroup) const;

	// Tries to change the activation group.  Returns true if it successfully changed.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Ascendunt|Ability", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool ChangeActivationGroup(EAscenduntAbilityActivationGroup NewGroup);

	
	// Sets the ability's camera mode.
	UFUNCTION(BlueprintCallable, Category = "Ascencunt|Ability")
	void SetCameraMode(TSubclassOf<UAscenduntCameraMode> CameraMode);

	// Clears the ability's camera mode.  Automatically called if needed when the ability ends.
	UFUNCTION(BlueprintCallable, Category = "Ascencunt|Ability")
	void ClearCameraMode();


protected:
	// --- UGameplayAbility overrides ---
	virtual bool DoesAbilitySatisfyTagRequirements(
		const UAbilitySystemComponent& AbilitySystemComponent,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void SetCanBeCanceled(bool bCanBeCanceled) override;

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual FGameplayEffectContextHandle MakeEffectContext(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const override;


	// AbilitySource(Weapon 등) 추출
	virtual void GetAbilitySource(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		float& OutSourceLevel, const IAscenduntAbilitySourceInterface*& OutAbilitySource, AActor*& OutEffectCauser) const;

	// BP hooks
	UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Ability", DisplayName = "OnAbilityAdded")
	void K2_OnAbilityAdded();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Ability", DisplayName = "OnAbilityRemoved")
	void K2_OnAbilityRemoved();

protected:
	// Defines how this ability is meant to activate.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Ability Activation")
	EAscenduntAbilityActivationPolicy ActivationPolicy;

	// Defines the relationship between this ability activating and other abilities activating.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Ability Activation")
	EAscenduntAbilityActivationGroup ActivationGroup;

	// Current camera mode set by the ability.
	TSubclassOf<UAscenduntCameraMode> ActiveCameraMode;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AscenduntHealthComponent.generated.h"

class UAscenduntHealthSet;
class UAscenduntAbilitySystemComponent;
struct FAscenduntHealthSnapshot;
struct FGameplayEffectSpec;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAscenduntHealth_DeathEvent,
	AActor*, OwningActor/*,
	AActor*, Instigator*/);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(
	FAscenduntHealth_AttributeChanged,
	UAscenduntHealthComponent*, HealthComponent,
	AActor*, OwningActor,
	AActor*, Instigator,
	bool, bWasDamaging,
	float, RawValue,
	const FAscenduntHealthSnapshot&, HealthSnapshot);

/**
 * EAscenduntDeathState
 *
 *	Defines current state of death.
 */
UENUM(BlueprintType)
enum class EAscenduntDeathState : uint8
{
	NotDead = 0,
	DeathStarted,
	DeathFinished
};


UCLASS(Blueprintable, Meta = (BlueprintSpawnableComponent))
class ASCENDUNT_API UAscenduntHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAscenduntHealthComponent(const FObjectInitializer& ObjectInitializer);

	// Returns the health component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Health")
	static UAscenduntHealthComponent* FindHealthComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<UAscenduntHealthComponent>() : nullptr); }

	// Initialize the component using an ability system component.
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health")
	bool InitializeWithAbilitySystem(UAscenduntAbilitySystemComponent* InASC);

	// Uninitialize the component, clearing any references to the ability system.
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health")
	void UninitializeFromAbilitySystem();

	// Returns the current health value.
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health")
	float GetHealth() const;

	// Returns the current maximum health value.
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health")
	float GetMaxHealth() const;

	// Returns the current health in the range [0.0, 1.0].
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health")
	float GetHealthNormalized() const;

	/** Copies all health pools for local presentation. */
	bool GetHealthSnapshot(FAscenduntHealthSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health")
	EAscenduntDeathState GetDeathState() const { return DeathState; }

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Ascendunt|Health", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool IsDeadOrDying() const { return (DeathState > EAscenduntDeathState::NotDead); }

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health")
	bool HasValidHealthSet() const;

	/**
	 * Resolves the persistent HealthSet owned by the ASC owner and keeps delegates
	 * synchronized. The Set is not removed during hero/weapon replacement.
	 */
	bool RefreshHealthSetFromAbilitySystem();

	// Begins the death sequence for the owner.
	virtual void StartDeath();

	// Ends the death sequence for the owner.
	virtual void FinishDeath();

	/** Server-only PvE revive transition: DeathStarted -> NotDead. Health must already be restored. */
	bool ReviveFromDeathStarted();

	/**
	 * Default true preserves the legacy AI flow (StartDeath -> FinishDeath immediately).
	 * HeroInitComponent disables it so the player can enter ragdoll before FinishDeath.
	 */
	void SetAutoFinishDeath(bool bInAutoFinishDeath) { bAutoFinishDeath = bInAutoFinishDeath; }

	// Applies enough damage to kill the owner.
	virtual void DamageSelfDestruct(bool bFellOutOfWorld = false);
public:

	// Delegate fired when the health value has changed. This is called on the client but the instigator may not be valid
	UPROPERTY(BlueprintAssignable)
	FAscenduntHealth_AttributeChanged OnHealthChanged;

	// Delegate fired when the death sequence has started.
	UPROPERTY(BlueprintAssignable)
	FAscenduntHealth_DeathEvent OnDeathStarted;

	// Delegate fired when the death sequence has finished.
	UPROPERTY(BlueprintAssignable)
	FAscenduntHealth_DeathEvent OnDeathFinished;

protected:

	virtual void OnUnregister() override;

	void ClearGameplayTags();

	virtual void HandleHealthChanged(
		AActor* Target, AActor* DamageInstigator,
		bool bWasDamaing, const FGameplayEffectSpec* DamageEffectSpec,
		float DamageOrHeal,	const FAscenduntHealthSnapshot& HealthSnapshot);

	virtual void HandleOutOfHealth(
		AActor* Target, AActor* DamageInstigator,
		const FGameplayEffectSpec* DamageEffectSpec);

	UFUNCTION()
	virtual void OnRep_DeathState(EAscenduntDeathState OldDeathState);

protected:

	// Ability system used by this component.
	UPROPERTY()
	TObjectPtr<UAscenduntAbilitySystemComponent> AbilitySystemComponent;

	// Health set used by this component.
	UPROPERTY()
	TObjectPtr<const UAscenduntHealthSet> HealthSet;

	// Replicated state used to handle dying.
	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	EAscenduntDeathState DeathState;

	bool bAutoFinishDeath = true;
};

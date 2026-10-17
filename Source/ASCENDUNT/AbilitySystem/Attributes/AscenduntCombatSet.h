// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystemComponent.h"
#include "CoreMinimal.h"
#include "AscenduntAttributeSet.h"
#include "AscenduntCombatSet.generated.h"

/**
* UAscenduntCombatSet
* 
*  Class that defines attributes that are necessary for applying damage or healing.
* Attribute examples include: damage, healing, attack power, and shield penetrations.
*/
UCLASS()
class ASCENDUNT_API UAscenduntCombatSet : public UAscenduntAttributeSet
{
	GENERATED_BODY()
	
public:
	UAscenduntCombatSet();

	ATTRIBUTE_ACCESSORS(UAscenduntCombatSet, BaseDamage);
	ATTRIBUTE_ACCESSORS(UAscenduntCombatSet, BaseHeal);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_BaseDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_BaseHeal(const FGameplayAttributeData& OldValue);

private:

	// The base amount of damage to apply in the damage execution.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BaseDamage, Category = "Ascendunt|Combat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BaseDamage;

	// The base amount of healing to apply in the heal execution.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BaseHeal, Category = "Ascendunt|Combat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BaseHeal;
};

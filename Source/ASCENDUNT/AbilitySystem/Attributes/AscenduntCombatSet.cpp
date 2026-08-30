// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntCombatSet.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

UAscenduntCombatSet::UAscenduntCombatSet()
	: BaseDamage(0.0f)
	, BaseHeal(0.0f)
{
}

void UAscenduntCombatSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntCombatSet, BaseDamage, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntCombatSet, BaseHeal, COND_OwnerOnly, REPNOTIFY_Always);
}

void UAscenduntCombatSet::OnRep_BaseDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntCombatSet, BaseDamage, OldValue);
}

void UAscenduntCombatSet::OnRep_BaseHeal(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntCombatSet, BaseHeal, OldValue);
}

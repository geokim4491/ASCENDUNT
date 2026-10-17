// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntAttributeSet.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"

UAscenduntAttributeSet::UAscenduntAttributeSet()
{
}

UWorld* UAscenduntAttributeSet::GetWorld() const
{
	const UObject* Outer = GetOuter();
	check(Outer);
	return Outer->GetWorld();
}

UAscenduntAbilitySystemComponent* UAscenduntAttributeSet::GetAscenduntAbilitySystemComponent() const
{
	return Cast<UAscenduntAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}

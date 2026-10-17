// Fill out your copyright notice in the Description page of Project Settings.

#include "AscenduntAbilitySystemGlobals.h"

#include "ASCENDUNT/AbilitySystem/AscenduntGameplayEffectContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AscenduntAbilitySystemGlobals)

FGameplayEffectContext* UAscenduntAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FAscenduntGameplayEffectContext();
}

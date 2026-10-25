// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystemGlobals.h"
#include "AscenduntAbilitySystemGlobals.generated.h"

class UObject;
struct FGameplayEffectContext;

/**
 * 
 */
UCLASS(Config = Game)
class ASCENDUNT_API UAscenduntAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()

	//~UAbilitySystemGlobals interface
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
	//~End of UAbilitySystemGlobals interface
	
};

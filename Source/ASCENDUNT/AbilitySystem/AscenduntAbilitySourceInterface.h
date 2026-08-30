// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/Interface.h"

#include "AscenduntAbilitySourceInterface.generated.h"

class UObject;
struct FGameplayTagContainer;

/** Base interface for anything acting as a ability calculation source */
UINTERFACE()
class UAscenduntAbilitySourceInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ASCENDUNT_API IAscenduntAbilitySourceInterface
{
	GENERATED_BODY()

public:
	/**
		 * Compute the multiplier for effect falloff with distance
		 *
		 * @param Distance			Distance from source to target for ability calculations (distance bullet traveled for a gun, etc...)
		 * @param SourceTags		Aggregated Tags from the source
		 * @param TargetTags		Aggregated Tags currently on the target
		 *
		 * @return Multiplier to apply to the base attribute value due to distance
		 */
	virtual float GetDistanceAttenuation(
		float Distance, const FGameplayTagContainer* SourceTags = nullptr
		, const FGameplayTagContainer* TargetTags = nullptr
	) const = 0;

	virtual float GetHeadshotDamageMultiplier(
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr
	) const = 0;
};

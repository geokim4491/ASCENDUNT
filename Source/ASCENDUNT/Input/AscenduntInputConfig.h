// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameplayTagContainer.h"

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AscenduntInputConfig.generated.h"

class UInputAction;
USTRUCT(BlueprintType)
struct FAscenduntInputAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * One-shot axis input that routes the positive and negative directions to
 * different, character-independent ability input tags.
 *
 * Example:
 * IA_Scroll > 0 -> InputTag.Scroll.Up
 * IA_Scroll < 0 -> InputTag.Scroll.Down
 */
USTRUCT(BlueprintType)
struct FAscenduntAxisAbilityInputAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (Categories = "InputTag"))
	FGameplayTag PositiveInputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (Categories = "InputTag"))
	FGameplayTag NegativeInputTag;
};

/**
 * 
 */
UCLASS(BlueprintType)
class ASCENDUNT_API UAscenduntInputConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UAscenduntInputConfig(const FObjectInitializer& ObjectInitializer);
	
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Pawn")
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Pawn")
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

public:
	// List of input actions used by the owner.  These input actions are mapped to a gameplay tag and must be manually bound.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FAscenduntInputAction> NativeInputActions;

	// List of digital input actions automatically bound to abilities with matching input tags.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FAscenduntInputAction> AbilityInputActions;

	// List of one-shot axis actions. Positive and negative values route to separate ability input tags.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FAscenduntAxisAbilityInputAction> AxisAbilityInputActions;
	
};

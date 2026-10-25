// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/DataAsset.h"

#include "HeroPawnData.generated.h"

class APawn;
class UObject;
class UAscenduntInputConfig;
class UAscenduntAbilitySet;
class UAbilityTagRelationshipMapping;
class UAscenduntCameraMode;
class UAnimInstance;

USTRUCT(BlueprintType)
struct FAscUltimateChargeConfig
{
	GENERATED_BODY()

	// Final applied contribution -> ultimate resource conversion. Overkill/overheal do not contribute.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ultimate", meta = (ClampMin = "0.0"))
	float DamageChargePerPoint = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ultimate", meta = (ClampMin = "0.0"))
	float HealingChargePerPoint = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ultimate", meta = (ClampMin = "1.0"))
	float MaxCharge = 100.0f;
};

/**
 * UHeroPawnData
 *
 * Non-mutable data asset that contains properties used to define a pawn.
 */
UCLASS(BlueprintType, Const, meta = (DisplayName = "Hero Pawn Data", ShortTooltip = "Data asset used to define a Pawn."))
class ASCENDUNT_API UHeroPawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UHeroPawnData(const FObjectInitializer& ObjectInitializer);

public:
	// Class to instantiate for this pawn (should usually derive from AAscenduntPawn or AAscenduntCharacter).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Pawn")
	TSubclassOf<APawn> PawnClass;


	// Ability sets to grant to this pawn's ability system.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Abilities")
	TArray<TObjectPtr<UAscenduntAbilitySet>> AbilitySets;


	// What mapping of ability tags to use for actions taking by this pawn
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Abilities")
	TObjectPtr<UAbilityTagRelationshipMapping> TagRelationshipMapping;

	// Input configuration used by player controlled pawns to create input mappings and bind input actions.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Input")
	TObjectPtr<UAscenduntInputConfig> InputConfig;


	// Default camera mode used by player controlled pawns.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Camera")
	TSubclassOf<UAscenduntCameraMode> DefaultCameraMode;

	// Hero-specific conversion from final combat contribution to the shared UltimateSet resource.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Ultimate")
	FAscUltimateChargeConfig UltimateCharge;
};

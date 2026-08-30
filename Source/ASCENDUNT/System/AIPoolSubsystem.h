// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIPoolSubsystem.generated.h"

class ABaseAICharacter;

USTRUCT(BlueprintType)
struct FAIPoolArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<ABaseAICharacter*> Characters;
};

/**
 * UAIPoolSubsystem
 * 
 * Manages the pooling of AI characters to prevent hitching from spawning/destroying during gameplay.
 */
UCLASS()
class ASCENDUNT_API UAIPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Request an AI from the pool. Spawns a new one if the pool for that class is empty.
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|Pool")
	ABaseAICharacter* AcquireAI(TSubclassOf<ABaseAICharacter> ClassToSpawn, const FTransform& SpawnTransform);

	// Returns an AI to the pool, hiding it and disabling its logic.
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|Pool")
	void ReleaseAI(ABaseAICharacter* AICharacter);

protected:
	// A map holding arrays of inactive AI characters, keyed by their class type.
	UPROPERTY()
	TMap<UClass*, FAIPoolArray> InactivePool;
};

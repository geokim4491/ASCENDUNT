// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "AscVisionLocalPlayerSubsystem.generated.h"

class AAscenduntWeapon;
class ABaseAICharacter;
class ABaseCharacter;
class AAscenduntPlayerState;
class UAscenduntHealthComponent;
class UAscTargetAwarenessComponent;

UCLASS()
class ASCENDUNT_API UAscVisionLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	// Re-evaluate visuals only when the local controller, PlayerState or Pawn changes.
	void NotifyLocalContextChanged();
	void NotifyCharacterBeganPlay(ABaseCharacter* Character);
	void NotifyCharacterPlayerStateChanged(ABaseCharacter* Character);

private:
	enum class ELocalVisionRelation : uint8
	{
		Invalid,
		Local,
		Ally,
		Enemy
	};

	static constexpr float CriticalHealthThreshold = 0.4f;

	void HandlePostWorldInit(UWorld* World, const UWorld::InitializationValues);
	void AttachToWorld(UWorld* World);
	void DetachFromWorld();
	bool RefreshLocalContext();


	void RegisterCharacter(ABaseCharacter* Character);
	void UnregisterCharacter(ABaseCharacter* Character);
	void HandleActorSpawned(AActor* SpawnedActor);

	UFUNCTION()
	void HandleTrackedActorDestroyed(AActor* DestroyedActor);


	void RefreshAllVisuals();
	void RefreshVisualsForCharacter(ABaseCharacter* Character);
	ELocalVisionRelation GetRelationToLocal(const ABaseCharacter* Target) const;


	void HandleLocalTeamChanged(uint8 OldTeamId, uint8 NewTeamId);
	void HandleObservedTargetsChanged();
	void HandleTrackedWeaponChanged(AAscenduntWeapon* NewWeapon, AAscenduntWeapon* PreviousWeapon);
	void HandleTrackedDownedStateChanged(bool bIsDowned);
	void HandleTrackedPooledActiveChanged(ABaseAICharacter* Character, bool bActive);


	UFUNCTION()
	void HandleTrackedHealthChanged(
		UAscenduntHealthComponent* HealthComponent,
		AActor* OwningActor,
		AActor* Instigator,
		bool bWasDamaging,
		float RawValue,
		const FAscenduntHealthSnapshot& HealthSnapshot);

	UFUNCTION()
	void HandleTrackedLifeStateChanged(AActor* OwningActor);

	UFUNCTION()
	void HandleLocalPawnChanged(
		APawn* OldPawn, APawn* NewPawn);

private:
	TSet<TWeakObjectPtr<ABaseCharacter>> TrackedCharacters;

	TWeakObjectPtr<UWorld> BoundWorld;
	TWeakObjectPtr<APlayerController> CachedPlayerController;
	TWeakObjectPtr<AAscenduntPlayerState> CachedLocalPlayerState;
	TWeakObjectPtr<ABaseCharacter> CachedLocalCharacter;
	TWeakObjectPtr<UAscTargetAwarenessComponent> CachedTargetAwarenessComponent;

	FDelegateHandle PostWorldInitHandle;
	FDelegateHandle ActorSpawnedHandle;
};

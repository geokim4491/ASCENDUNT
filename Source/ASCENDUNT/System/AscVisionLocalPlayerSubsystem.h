// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "AscVisionLocalPlayerSubsystem.generated.h"

class ABaseCharacter;
class AAscenduntPlayerState;
class UAscenduntHealthComponent;
class UMaterialInterface;
class UMeshComponent;
class UPrimitiveComponent;
class UAscTargetAwarenessComponent;
class USkeletalMeshComponent;

/**
 * Local-only vision rendering controller.
 *
 * Mesh-based X-Ray path:
 * - Remote friendly characters render pre-created local X-Ray mesh components.
 * - Local player meshes are written to CustomDepth only as blockers.
 */
UCLASS()
class ASCENDUNT_API UAscVisionLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

private:
	static constexpr int32 XRayHealthPrimitiveDataIndex = 0;
	static constexpr int32 XRayAlphaPrimitiveDataIndex = 1;

	void HandlePostWorldInit(UWorld* World, const UWorld::InitializationValues IVS);
	void AttachToWorld(UWorld* World);
	void DetachFromWorld();

	void RefreshLocalContext();
	void RefreshTrackedCharacters();
	void RefreshAllVisuals();
	void RefreshVisualsForCharacter(ABaseCharacter* Character);

	void RegisterCharacter(ABaseCharacter* Character);
	void UnregisterCharacter(ABaseCharacter* Character);
	void HandleActorSpawned(AActor* SpawnedActor);

	ABaseCharacter* GetLocalBaseCharacter() const;
	bool IsFriendlyToLocal(const ABaseCharacter* Target) const;
	bool IsEnemyToLocal(const ABaseCharacter* Target) const;
	bool IsPlayerCharacter(const ABaseCharacter* Target) const;

	float GetTargetHealthNormalized(const ABaseCharacter* Character) const;
	bool IsEnemyHealthBarRevealed(const ABaseCharacter* Target) const;
	void UpdateTargetHealthBar(ABaseCharacter* Character, bool bShouldShow, bool bFriendly) const;

	void HandleLocalTeamChanged(uint8 OldTeamId, uint8 NewTeamId);
	void HandleObservedTargetsChanged();

	void ApplyDisabledState(ABaseCharacter* Character, bool bShowCriticalWidget);
	void ApplyLocalBlockerState(ABaseCharacter* Character);
	void ApplyAllyXRayState(ABaseCharacter* Character, float HealthNorm, bool bShowCriticalWidget);
	void ApplyEnemyOverlayState(ABaseCharacter* Character);

	void SetMeshBlockerCustomDepth(UMeshComponent* Mesh, bool bEnableCustomDepth) const;
	void SetCharacterBlockerCustomDepth(ABaseCharacter* Character, bool bEnableCustomDepth) const;

	void SetMeshXRayEnabled(UMeshComponent* Mesh, bool bEnabled, float HealthNorm = 1.f, float XRayAlpha = 1.f) const;
	void SetCharacterXRayEnabled(ABaseCharacter* Character, bool bEnabled, float HealthNorm = 1.f, float XRayAlpha = 1.f) const;

	float ComputeCriticalWarningScale(const ABaseCharacter* Target) const;
	void UpdateCriticalWarningScale(ABaseCharacter* Character, bool bShowCriticalWidget) const;

	UFUNCTION()
	void HandleTrackedHealthChanged(
		UAscenduntHealthComponent* HealthComponent,
		AActor* OwningActor,
		AActor* Instigator,
		bool bWasDamaging,
		float RawValue,
		const FAscenduntHealthSnapshot& HealthSnapshot);

	UFUNCTION()
	void HandleTrackedDeathStarted(AActor* OwningActor);

	UFUNCTION()
	void HandleTrackedDeathFinished(AActor* OwningActor);

private:
	TSet<TWeakObjectPtr<ABaseCharacter>> TrackedCharacters;

	TWeakObjectPtr<UWorld> BoundWorld;
	TWeakObjectPtr<APlayerController> CachedPlayerController;
	TWeakObjectPtr<AAscenduntPlayerState> CachedLocalPlayerState;
	TWeakObjectPtr<UAscTargetAwarenessComponent> CachedTargetAwarenessComponent;

	FDelegateHandle PostWorldInitHandle;
	FDelegateHandle ActorSpawnedHandle;
	FTimerHandle RefreshTimerHandle;

	float RefreshInterval = 0.15f;
	float CriticalThreshold = 0.4f;
	bool bTreatNonPlayerCharactersAsHostile = true;

	// 언제부터 줄일지
	float CriticalScaleNearDistance = 300.f;
	
	// 최소 크기가 되는 지점
	float CriticalScaleFarDistance = 2200.f;
	float CriticalScaleMax = 1.0f;
	float CriticalScaleMin = 0.3f;
};

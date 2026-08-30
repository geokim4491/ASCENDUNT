#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "HakukoSkillComponent.generated.h"

class AHakukoCharacter;
class UNiagaraSystem;
class UNiagaraComponent;
class UAscCombatContributionComponent;
class UWeaponEquipmentComponent;
class AAscenduntWeapon;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHakukoStimTargetChanged, AActor*, TargetActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHakukoHealGaugeChanged, float, NewGauge);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHakukoUltimateStateChanged, bool, bActive);

/** Hakuko-specific runtime state. Combat totals/ultimate resource itself live on PlayerState. */
UCLASS(ClassGroup=(ASCENDUNT), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UHakukoSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHakukoSkillComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeForGameplay();
	void DeinitializeForGameplay();

	UFUNCTION(BlueprintPure, Category="Hakuko|Stim")
	AActor* GetLocalStimTargetActor() const { return LocalStimTargetActor.Get(); }

	/** Server recomputes the current aim target; gameplay never trusts client target state. */
	AActor* FindBestFriendlyTarget_Server() const;

	UFUNCTION(BlueprintPure, Category="Hakuko|Heal Grenade")
	float GetHealGrenadeGauge() const { return HealGrenadeGauge; }

	float CalculateHealGrenadeAmount(float MinHeal, float MaxHeal) const;
	void ResetHealGrenadeGauge_Server();

	void ActivateUltimate_Server(float Duration, float AOERadius, float AOEDamageCoefficient, UNiagaraSystem* WeaponVFX, bool bDebugDrawAOERadius, float AOEDebugDrawDuration);
	void DeactivateUltimate_Server();
	bool GetUltimateAOEParameters(float& OutRadius, float& OutDamageCoefficient, bool& bOutDebugDrawRadius, float& OutDebugDrawDuration) const;

	UPROPERTY(BlueprintAssignable, Category="Hakuko|Stim")
	FOnHakukoStimTargetChanged OnStimTargetChanged;

	UPROPERTY(BlueprintAssignable, Category="Hakuko|Heal Grenade")
	FOnHakukoHealGaugeChanged OnHealGrenadeGaugeChanged;

	UPROPERTY(BlueprintAssignable, Category="Hakuko|Ultimate")
	FOnHakukoUltimateStateChanged OnUltimateStateChanged;

protected:
	UPROPERTY(EditAnywhere, Category="Hakuko|Stim|Targeting", meta=(ClampMin="0.0"))
	float TargetingRange = 3000.0f;

	UPROPERTY(EditAnywhere, Category="Hakuko|Stim|Targeting", meta=(ClampMin="0.1", ClampMax="89.0"))
	float TargetingHalfAngleDegrees = 8.0f;

	UPROPERTY(EditAnywhere, Category="Hakuko|Stim|Targeting")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditAnywhere, Category="Hakuko|Stim|Targeting")
	TEnumAsByte<ECollisionChannel> LineOfSightTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category="Hakuko|Stim|Targeting")
	FVector TargetPointOffset = FVector(0.0f, 0.0f, 70.0f);

	/** Amount of actual enemy damage required to fill the secondary-heal gauge from 0 to 100. */
	UPROPERTY(EditAnywhere, Category="Hakuko|Heal Grenade", meta=(ClampMin="1.0"))
	float DamageForFullHealGauge = 500.0f;

private:
	AHakukoCharacter* GetHakukoOwner() const;
	bool ShouldRunLocalTargeting() const;
	AActor* FindBestFriendlyTarget(bool bServerQuery) const;
	bool IsCandidateVisible(const FVector& ViewLocation, AActor* Candidate) const;
	void UpdateLocalTarget();
	void SetLocalTarget(AActor* NewTarget);
	void HandleDamageDealt(float ActualDamage);
	void RefreshUltimateWeaponVFX();
	void ClearUltimateWeaponVFX();
	void HandleWeaponChanged(AAscenduntWeapon* NewWeapon, AAscenduntWeapon* PreviousWeapon);

	UFUNCTION()
	void OnRep_HealGrenadeGauge();

	UFUNCTION()
	void OnRep_UltimateState();

private:
	TWeakObjectPtr<AActor> LocalStimTargetActor;
	TWeakObjectPtr<UAscCombatContributionComponent> BoundContribution;
	FDelegateHandle DamageDealtDelegateHandle;
	FDelegateHandle WeaponChangedDelegateHandle;

	UPROPERTY(ReplicatedUsing=OnRep_HealGrenadeGauge, Transient)
	float HealGrenadeGauge = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_UltimateState, Transient)
	bool bUltimateActive = false;

	UPROPERTY(ReplicatedUsing=OnRep_UltimateState, Transient)
	float UltimateAOERadius = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_UltimateState, Transient)
	float UltimateAOEDamageCoefficient = 1.0f;

	// Server-side shot snapshot settings. The actual debug sphere is multicast by the weapon at impact.
	bool bUltimateDebugDrawAOERadius = false;
	float UltimateAOEDebugDrawDuration = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_UltimateState, Transient)
	TObjectPtr<UNiagaraSystem> UltimateWeaponVFX = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> UltimateWeaponVFXComponent = nullptr;

	FTimerHandle UltimateTimer;
	bool bGameplayInitialized = false;
};

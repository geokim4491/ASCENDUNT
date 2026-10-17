#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AscCombatContributionComponent.generated.h"

class UHeroPawnData;
class UAscenduntAbilitySystemComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAscDamageDealtNative, float /*ActualDamage*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAscHealingDoneNative, float /*ActualHealing*/);
DECLARE_MULTICAST_DELEGATE(FOnAscKillNative);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAscCombatValueChanged, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAscCombatKillsChanged, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAscCombatContributionChanged);

/**
 * Server-authoritative, persistent combat contribution ledger owned by PlayerState.
 *
 * HealthSet reports only final applied damage/healing values. Weapon fire reports
 * authoritative fired/hit counters. PvE Downed/Revive reports life-state counters.
 * The component replicates the resulting totals so every client can build the TAB scoreboard
 * without any UI RPC or client-side gameplay accounting.
 */
UCLASS(ClassGroup=(ASCENDUNT), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UAscCombatContributionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAscCombatContributionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Called at Hero GASInitialized. Reconfigures coefficients and resets only hero-specific Ultimate charge. */
	void InitializeForPawnData(const UHeroPawnData* PawnData, UAscenduntAbilitySystemComponent* ASC);

	// ===== Authoritative record API =====
	void RecordDamageDealt(float ActualDamage, bool bKilledTarget);
	void RecordHealingDone(float ActualHealing);
	void RecordHealingReceived(float ActualHealing);
	void RecordDowned();
	void RecordRevive();
	void RecordWeaponShotsFired(int32 Count = 1);
	void RecordWeaponHit(bool bCriticalHit);

	// ===== Scoreboard getters =====
	UFUNCTION(BlueprintPure, Category="Ascendunt|Combat Contribution")
	float GetTotalDamageDealt() const { return TotalDamageDealt; }

	UFUNCTION(BlueprintPure, Category="Ascendunt|Combat Contribution")
	float GetTotalHealingDone() const { return TotalHealingDone; }

	UFUNCTION(BlueprintPure, Category="Ascendunt|Combat Contribution")
	float GetTotalHealingReceived() const { return TotalHealingReceived; }

	/** In PvE this is the monster kill count. */
	UFUNCTION(BlueprintPure, Category="Ascendunt|Combat Contribution")
	int32 GetTotalKills() const { return TotalKills; }

	UFUNCTION(BlueprintPure, Category="Ascendunt|Combat Contribution")
	int32 GetDownedCount() const { return DownedCount; }

	UFUNCTION(BlueprintPure, Category="Ascendunt|Combat Contribution")
	int32 GetReviveCount() const { return ReviveCount; }

	UFUNCTION(BlueprintPure, Category="Ascendunt|Combat Contribution")
	int32 GetWeaponShotsFired() const { return WeaponShotsFired; }

	UFUNCTION(BlueprintPure, Category="Ascendunt|Combat Contribution")
	int32 GetWeaponHits() const { return WeaponHits; }

	UFUNCTION(BlueprintPure, Category="Ascendunt|Combat Contribution")
	int32 GetCriticalWeaponHits() const { return CriticalWeaponHits; }

	/** Weapon hits / weapon shots. Returned in 0..100 percentage units. */
	UFUNCTION(BlueprintPure, Category="Ascendunt|Combat Contribution")
	float GetWeaponAccuracyPercent() const;

	/** Critical weapon hits / weapon hits, matching Overwatch-style critical-hit accuracy. */
	UFUNCTION(BlueprintPure, Category="Ascendunt|Combat Contribution")
	float GetCriticalHitAccuracyPercent() const;

	void ResetContribution();

	FOnAscDamageDealtNative OnDamageDealtNative;
	FOnAscHealingDoneNative OnHealingDoneNative;
	FOnAscKillNative OnKillNative;

	UPROPERTY(BlueprintAssignable, Category="Ascendunt|Combat Contribution")
	FOnAscCombatValueChanged OnTotalDamageDealtChanged;

	UPROPERTY(BlueprintAssignable, Category="Ascendunt|Combat Contribution")
	FOnAscCombatValueChanged OnTotalHealingDoneChanged;

	UPROPERTY(BlueprintAssignable, Category="Ascendunt|Combat Contribution")
	FOnAscCombatKillsChanged OnTotalKillsChanged;

	/** Generic local UI invalidation signal for the TAB scoreboard. */
	UPROPERTY(BlueprintAssignable, Category="Ascendunt|Combat Contribution")
	FOnAscCombatContributionChanged OnContributionChanged;

private:
	void AddUltimateCharge(float Amount);
	void BroadcastContributionChanged();

	UFUNCTION() void OnRep_TotalDamageDealt();
	UFUNCTION() void OnRep_TotalHealingDone();
	UFUNCTION() void OnRep_TotalHealingReceived();
	UFUNCTION() void OnRep_TotalKills();
	UFUNCTION() void OnRep_DownedCount();
	UFUNCTION() void OnRep_ReviveCount();
	UFUNCTION() void OnRep_WeaponShotsFired();
	UFUNCTION() void OnRep_WeaponHits();
	UFUNCTION() void OnRep_CriticalWeaponHits();

private:
	UPROPERTY(ReplicatedUsing=OnRep_TotalDamageDealt)
	float TotalDamageDealt = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_TotalHealingDone)
	float TotalHealingDone = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_TotalHealingReceived)
	float TotalHealingReceived = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_TotalKills)
	int32 TotalKills = 0;

	UPROPERTY(ReplicatedUsing=OnRep_DownedCount)
	int32 DownedCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_ReviveCount)
	int32 ReviveCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_WeaponShotsFired)
	int32 WeaponShotsFired = 0;

	UPROPERTY(ReplicatedUsing=OnRep_WeaponHits)
	int32 WeaponHits = 0;

	UPROPERTY(ReplicatedUsing=OnRep_CriticalWeaponHits)
	int32 CriticalWeaponHits = 0;

	UPROPERTY(Transient)
	TObjectPtr<const UHeroPawnData> ConfiguredPawnData = nullptr;
	TWeakObjectPtr<UAscenduntAbilitySystemComponent> CachedASC;

	float DamageChargePerPoint = 0.0f;
	float HealingChargePerPoint = 0.0f;
};

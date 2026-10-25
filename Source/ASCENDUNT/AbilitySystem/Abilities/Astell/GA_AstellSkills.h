#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Common/GA_DataAbility.h"
#include "GA_AstellSkills.generated.h"

class UAstellSwordWaveData;
class UAstellDashData;
class UAstellExplosionData;
class UAstellLeapData;
class UAstellUltData;
class UAscSkillStockComponent;
class AAscenduntProjectile;
class AActor;

UCLASS()
class ASCENDUNT_API UGA_AstellSwordWave : public UGA_DataAbility
{
	GENERATED_BODY()

public:
	UGA_AstellSwordWave(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool GetSkillStockDefinition(FGameplayTag& OutStockTag, int32& OutMaxStock, float& OutRecoverDuration) const override;

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual const UAscAbilityData* GetAbilityData() const override;

private:
	bool SpawnSwordWaveProjectile(const UAstellSwordWaveData* Data) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityData", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAstellSwordWaveData> SwordWaveData;
};

UCLASS()
class ASCENDUNT_API UGA_AstellDashSlash : public UGA_DataAbility
{
	GENERATED_BODY()

public:
	UGA_AstellDashSlash(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool GetSkillStockDefinition(FGameplayTag& OutStockTag, int32& OutMaxStock, float& OutRecoverDuration) const override;

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void OnDataAbilityMontageFinished(bool bWasCancelled) override;
	virtual const UAscAbilityData* GetAbilityData() const override;

private:
	void ScheduleDashTimer(FTimerHandle& TimerHandle, float Delay, void (UGA_AstellDashSlash::*Callback)());
	float GetDashScaledTime(float RawTime) const;
	float GetDashHitEndTime() const;

	void BeginDashHitWindow();
	void TickDashHitBox_Server();
	void EndDashHitWindow();

	void ClearDashTimers();

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityData", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAstellDashData> DashData;

	FTimerHandle DashHitStartTimer;
	FTimerHandle DashHitTickTimer;
	FTimerHandle DashHitEndTimer;

	bool bDashHitWindowActive = false;
	bool bDashFinishing = false;

	TSet<TWeakObjectPtr<AActor>> DashHitActors;
};

UCLASS()
class ASCENDUNT_API UGA_AstellHolyExplosion : public UGA_DataAbility
{
	GENERATED_BODY()

public:
	UGA_AstellHolyExplosion(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual const UAscAbilityData* GetAbilityData() const override;

private:
	void ScheduleExplosionTimer(FTimerHandle& TimerHandle, float Delay, void (UGA_AstellHolyExplosion::*Callback)());
	float GetExplosionScaledTime(float RawTime) const;
	void ApplyExplosionDamage_Server();
	void ApplyExplosionKnockback_Server();
	void ClearExplosionTimers();
	void DrawExplosionDebugSphere(const FColor& Color) const;
	void TryFinishExplosionAfterTimedEvents();

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityData", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAstellExplosionData> ExplosionData;

	FTimerHandle ExplosionDamageTimer;
	FTimerHandle ExplosionKnockbackTimer;

	TArray<TWeakObjectPtr<AActor>> ExplosionAffectedTargets;

	bool bExplosionWaitingForMontageEnd = false;
	bool bExplosionDamageResolved = false;
	bool bExplosionKnockbackResolved = false;
};

UCLASS()
class ASCENDUNT_API UGA_AstellLeap : public UGA_DataAbility
{
	GENERATED_BODY()

public:
	UGA_AstellLeap(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual const UAscAbilityData* GetAbilityData() const override;

private:
	UFUNCTION()
	void TryResetLeapAvailability();

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityData", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAstellLeapData> LeapData;

	mutable bool bLeapAvailable = true;
	FTimerHandle LeapResetTimer;
};

UCLASS()
class ASCENDUNT_API UGA_AstellUlt : public UGA_DataAbility
{
	GENERATED_BODY()

public:
	UGA_AstellUlt(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual const UAscAbilityData* GetAbilityData() const override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityData", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAstellUltData> UltData;

};

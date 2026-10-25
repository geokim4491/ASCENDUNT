// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Weapon/GA_WeaponAbilityBase.h"
#include "GA_Melee.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAT_PlayWeaponMontageAndWait;
struct FAscenduntMeleeStep;

/**
 * ModeStat.MeleeSteps 기반 근접 공격 Ability.
 * - MeleeSteps가 1개면 단일 근접 공격
 * - MeleeSteps가 여러 개면 ComboInputOpenTime 이후 공격 입력 시 즉시 다음 Step으로 전환
 * - AnimNotifyState_MeleeHitWindow는 HitBox 활성/비활성 타이밍만 담당
 */
UCLASS()
class ASCENDUNT_API UGA_Melee : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()
	
public:
	UGA_Melee();

	// 근접 공격 컨텍스트 생성. TryPrepareShot을 사용하지 않아 Fire 전용 로직과 분리한다.
	// 첫 공격은 Commit/FireGate를 확인하고, 콤보 Step 전환은 동일 Ability 내부 전환이므로 FireGate를 다시 확인하지 않는다.
	bool BuildMeleeAttackContext(float DamageCoef, int32 AttackIndex, FAscenduntWeaponAttackContext& OutCtx, bool bDoCommit, bool bCheckFireGate);

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	// Extension hook for hero-specific melee effects. Called only on the authoritative ability
	// after a melee damage impact has been confirmed and the damage GE has been applied.
	virtual void OnServerMeleeDamageHitConfirmed(AActor* HitActor, const FHitResult& Hit,
		const FAscenduntWeaponAttackContext& Context, const FAscenduntMeleeStep& Step, int32 StepIndex) {}

	virtual void OnMeleeHitWindowStarted(int32 StepIndex) {}
	virtual void OnMeleeHitWindowEnded(int32 StepIndex) {}

private:
	// 콤보 상태머신
	void StartComboStep(int32 StepIndex, bool bInitialStep);
	void OpenComboInput();
	void CloseComboInput();
	void TryAdvanceComboStepFromInput();
	void FinishMeleeAbility(bool bWasCancelled);
	void ResetComboState();

	// 몽타주
	void PlayMeleeStepMontages(const FAscenduntMeleeStep& Step, class AAscenduntWeapon* Weapon, float PlayRate, bool bReplicateWeaponMontage);
	void StopCurrentMeleeMontagesNoBlend();
	void ClearComboTimers();

	// 서버: HitWindow 이벤트 수신
	void BindWeaponHitWindow();
	void UnbindWeaponHitWindow();
	void OnWeaponHitWindow(bool bBegin, int32 NotifyStepIndex);

	// 서버: 히트 틱
	void StartServerHitTick();
	void StopServerHitTick();
	void ServerHitTick();

	// 데이터 접근
	bool GetMeleeStep(const class AAscenduntWeapon* Weapon, int32 StepIndex, const FAscenduntMeleeStep*& OutStep) const;
	float GetMeleePlayRate(const class AAscenduntWeapon* Weapon, const FAscenduntMeleeStep* Step) const;
	float GetMeleeComboGateSeconds(const class AAscenduntWeapon* Weapon, const FAscenduntMeleeStep* Step) const;
	bool HasNextComboStep(const class AAscenduntWeapon* Weapon) const;

private:
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> HeroMeleeMontageTask;

	UPROPERTY()
	TObjectPtr<UAT_PlayWeaponMontageAndWait> WeaponMeleeMontageTask;

	FTimerHandle ComboInputOpenTimer;
	FTimerHandle ComboInputCloseTimer;
	FTimerHandle ServerHitTickTimer;

	bool bComboInputOpen = false;
	bool bFinishingMelee = false;
	bool bInputReleased = false;
	bool bStopMontagesOnEnd = false;

	int32 AttackIndex = 0;
	int32 CurrentStepIndex = 0;

	// 현재 스윙 컨텍스트(서버 적용용)
	FAscenduntWeaponAttackContext SwingCtx;

	// 히트 윈도우 활성 여부(서버)
	bool bServerHitWindowActive = false;

	// 콤보 전환 직후 이전 몽타주의 NotifyEnd가 늦게 도착해 현재 Step HitWindow를 닫는 것을 막기 위한 추적 값.
	int32 ActiveHitWindowNotifyStepIndex = INDEX_NONE;
	int32 ActiveHitWindowAbilityStepIndex = INDEX_NONE;

	// 이번 윈도우 중복 타격 방지
	TSet<TWeakObjectPtr<AActor>> HitActorsThisWindow;

	// 현재 무기 바인딩
	TWeakObjectPtr<class AAscenduntWeapon> CachedWeapon;
	FDelegateHandle HitWindowHandle;
};

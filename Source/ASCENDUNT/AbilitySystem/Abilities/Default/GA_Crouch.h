// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/AscenduntGameplayAbility.h"
#include "GA_Crouch.generated.h"

/**
 *  Hold: crouch 유지
 *  Release: uncrouch 시도 후 종료 (서버에도 release 전달)
 */
UCLASS()
class ASCENDUNT_API UGA_Crouch : public UAscenduntGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_Crouch();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	
protected:
	// Ability active 동안 사용할 3인칭 CameraMode ======== 테스트 용 ========
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TSubclassOf<UAscenduntCameraMode> ThirdPersonCameraMode;

private:

	FTimerHandle RetryUncrouchTimer;

	void StartCrouch();
	void StopCrouch_TryAndRetry();   // Release시 호출
	bool TryUncrouchOnce();          // 성공/실패 반환
	void RetryUncrouchTick();
	void ClearRetryTimer();
	
};

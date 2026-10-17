// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AT_PlayWeaponMontageAndWait.generated.h"

class AAscenduntWeapon;
class UAnimMontage;
class UAnimInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAscWeaponMontageTaskSimple);

/**
 * 
 */
UCLASS()
class ASCENDUNT_API UAT_PlayWeaponMontageAndWait : public UAbilityTask
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintAssignable)
	FAscWeaponMontageTaskSimple OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FAscWeaponMontageTaskSimple OnInterrupted;

	static UAT_PlayWeaponMontageAndWait* PlayWeaponMontageAndWait(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		AAscenduntWeapon* Weapon,
		UAnimMontage* Montage,
		float PlayRate,
		bool bReplicateToOthers,
		bool bStopWhenAbilityEnds = true);

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void HandleMontageEnded(UAnimMontage* InMontage, bool bInterrupted);

private:
	UPROPERTY()
	TObjectPtr<AAscenduntWeapon> WeaponPtr;

	UPROPERTY()
	TObjectPtr<UAnimMontage> MontagePtr;

	UPROPERTY()
	TObjectPtr<UAnimInstance> AnimInstancePtr;

	float Rate = 1.f;
	bool bReplicate = false;
	bool bStopOnEnd = true;
	
	
};

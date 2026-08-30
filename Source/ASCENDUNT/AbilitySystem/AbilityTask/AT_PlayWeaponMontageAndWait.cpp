// Fill out your copyright notice in the Description page of Project Settings.


#include "AT_PlayWeaponMontageAndWait.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

UAT_PlayWeaponMontageAndWait* UAT_PlayWeaponMontageAndWait::PlayWeaponMontageAndWait(
	UGameplayAbility* OwningAbility, 
	FName TaskInstanceName, 
	AAscenduntWeapon* Weapon, 
	UAnimMontage* Montage, 
	float PlayRate, 
	bool bReplicateToOthers, 
	bool bStopWhenAbilityEnds)
{
	UAT_PlayWeaponMontageAndWait* Task = NewAbilityTask<UAT_PlayWeaponMontageAndWait>(OwningAbility, TaskInstanceName);
	Task->WeaponPtr = Weapon;
	Task->MontagePtr = Montage;
	Task->Rate = PlayRate;
	Task->bReplicate = bReplicateToOthers;
	Task->bStopOnEnd = bStopWhenAbilityEnds;
	return Task;
}

void UAT_PlayWeaponMontageAndWait::Activate()
{
	Super::Activate();

	if (!WeaponPtr || !MontagePtr)
	{
		EndTask();
		return;
	}

	USkeletalMeshComponent* Mesh = WeaponPtr->GetWeaponMesh();
	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		// StaticMesh 무기는 무기 자체 몽타주를 재생하지 않는다.
		EndTask();
		return;
	}

	AnimInstancePtr = Mesh->GetAnimInstance();
	if (AnimInstancePtr)
	{
		// 종료 콜백 바인딩
		FOnMontageEnded Ended;
		Ended.BindUObject(this, &ThisClass::HandleMontageEnded);
		AnimInstancePtr->Montage_SetEndDelegate(Ended, MontagePtr);

		AnimInstancePtr->Montage_Play(MontagePtr, Rate);
	}
	else
	{
		EndTask();
		return;
	}

	// 서버가 다른 클라들에게 “무기 몽타주 재생”을 알려줌
	if (bReplicate && WeaponPtr->HasAuthority())
	{
		WeaponPtr->Multicast_PlayWeaponMontage(MontagePtr, Rate);
	}
}

void UAT_PlayWeaponMontageAndWait::OnDestroy(bool bInOwnerFinished)
{
	if (bStopOnEnd && AnimInstancePtr && MontagePtr)
	{
		AnimInstancePtr->Montage_Stop(0.1f, MontagePtr);
	}

	Super::OnDestroy(bInOwnerFinished);
}

void UAT_PlayWeaponMontageAndWait::HandleMontageEnded(UAnimMontage* InMontage, bool bInterrupted)
{
	if (bInterrupted)
	{
		OnInterrupted.Broadcast();
	}
	else
	{
		OnCompleted.Broadcast();
	}
	EndTask();
}

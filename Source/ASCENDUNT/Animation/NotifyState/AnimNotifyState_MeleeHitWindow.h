// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_MeleeHitWindow.generated.h"

/**
 * 
 */
UCLASS(meta = (DisplayName = "Asc Melee Hit Window"))
class ASCENDUNT_API UAnimNotifyState_MeleeHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	// 이 윈도우가 어떤 MeleeStep을 의미하는지(기본 0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	int32 StepIndex = 0;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

private:
	class AAscenduntWeapon* FindWeapon(USkeletalMeshComponent* MeshComp) const;
	
	
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTT_PlayMontageDirect.generated.h"

/**
 * 
 */
UCLASS()
class ASCENDUNT_API UBTTask_PlayMontageDirect : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PlayMontageDirect();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	// 에디터에서 선택할 재생할 몽타주
	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimMontage> MontageToPlay;
};

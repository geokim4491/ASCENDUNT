#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_CheckDistance.generated.h"

UCLASS()
class ASCENDUNT_API UBTDecorator_CheckDistance : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_CheckDistance();

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

	// 에디터에서 설정할 공격 사거리
	UPROPERTY(EditAnywhere, Category = "Condition")
	float AttackRange = 250.0f;

	// 블랙보드에서 타겟(플레이어)을 읽어올 키
	UPROPERTY(EditAnywhere, Category = "Condition")
	FBlackboardKeySelector TargetKey;
};

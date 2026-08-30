#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "BTDecorator_CheckHealth.generated.h"

UCLASS()
class ASCENDUNT_API UBTDecorator_CheckHealth : public UBTDecorator_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTDecorator_CheckHealth();

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

protected:
	// The threshold percentage of health to check against (e.g. 30.0f for 30%)
	UPROPERTY(EditAnywhere, Category = "Health")
	float ThresholdPercentage;
};

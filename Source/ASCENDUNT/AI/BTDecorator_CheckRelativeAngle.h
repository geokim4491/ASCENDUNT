#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "BTDecorator_CheckRelativeAngle.generated.h"

UCLASS()
class ASCENDUNT_API UBTDecorator_CheckRelativeAngle : public UBTDecorator_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTDecorator_CheckRelativeAngle();

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

protected:
	// The minimum angle (-180 to 180)
	UPROPERTY(EditAnywhere, Category = "Angle")
	float MinAngle;

	// The maximum angle (-180 to 180). If MinAngle > MaxAngle, it checks for wrap-around (e.g. Back is 135 to -135)
	UPROPERTY(EditAnywhere, Category = "Angle")
	float MaxAngle;
};

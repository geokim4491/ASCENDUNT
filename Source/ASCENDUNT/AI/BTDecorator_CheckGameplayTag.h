#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "GameplayTagContainer.h"
#include "BTDecorator_CheckGameplayTag.generated.h"

UCLASS()
class ASCENDUNT_API UBTDecorator_CheckGameplayTag : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_CheckGameplayTag();

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
	// 검사할 태그 (예: State.HitReact)
	UPROPERTY(EditAnywhere, Category = "GAS")
	FGameplayTag TagToCheck;

	// 태그가 '있어야' true인지, '없어야' true인지
	UPROPERTY(EditAnywhere, Category = "GAS")
	bool bRequireTag;
};

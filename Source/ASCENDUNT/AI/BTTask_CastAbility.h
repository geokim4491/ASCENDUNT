#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "BTTask_CastAbility.generated.h"

UCLASS()
class ASCENDUNT_API UBTTask_CastAbility : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_CastAbility();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
    // BT 에디터에서 선택할 스킬 태그 (예: Ability.Attack.Melee)
    UPROPERTY(EditAnywhere, Category = "GAS")
    FGameplayTag AbilityTag;
};

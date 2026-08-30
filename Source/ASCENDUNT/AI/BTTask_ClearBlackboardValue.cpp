#include "BTTask_ClearBlackboardValue.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_ClearBlackboardValue::UBTTask_ClearBlackboardValue()
{
	NodeName = TEXT("Clear Blackboard Value");

	bCreateNodeInstance = false;

	// UBTTask_BlackboardBase 가 BlackboardKey 셀렉터를 제공한다.
	// 타입 필터를 걸지 않아 어떤 키든 비울 수 있다.
}

FString UBTTask_ClearBlackboardValue::GetStaticDescription() const
{
	return FString::Printf(TEXT("Clear: %s"), *BlackboardKey.SelectedKeyName.ToString());
}

EBTNodeResult::Type UBTTask_ClearBlackboardValue::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB || BlackboardKey.SelectedKeyName.IsNone())
	{
		return EBTNodeResult::Failed;
	}

	BB->ClearValue(BlackboardKey.SelectedKeyName);

	return EBTNodeResult::Succeeded;
}

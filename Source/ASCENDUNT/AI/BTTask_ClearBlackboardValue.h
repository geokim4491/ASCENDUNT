#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_ClearBlackboardValue.generated.h"

/**
 * 블랙보드 키를 비운다.
 *
 * 엔진은 값을 "설정"하는 태스크(Set Bool Key 등)만 제공하고 비우는 태스크는 없다.
 * Vector 키의 경우 아무 값이나 넣으면 Is Set 조건이 계속 참이라, 명시적으로
 * 비워야 그 키를 조건으로 쓰는 브랜치가 닫힌다.
 *
 * 용례: 마지막 목격 위치까지 가서 탐색을 마친 뒤 LastKnownLocation 을 비운다.
 * 비우지 않으면 같은 지점을 무한히 다시 찾아간다.
 */
UCLASS(DisplayName = "Clear Blackboard Value")
class ASCENDUNT_API UBTTask_ClearBlackboardValue : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_ClearBlackboardValue();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
};

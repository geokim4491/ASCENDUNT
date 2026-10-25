#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindRandomPatrolLocation.generated.h"

/**
 * 순찰 목적지를 뽑아 블랙보드에 기록한다. 이동은 뒤따르는 Move To 가 맡는다.
 *
 * 기준점(Origin)은 트리가 시작될 때의 폰 위치를 한 번 캡처해 계속 재사용한다.
 * 매번 현재 위치를 기준으로 잡으면 랜덤 워크가 되어 AI 가 스폰 지점에서
 * 한없이 멀어진다. 기준점을 고정해야 "자기 구역을 도는" 행동이 된다.
 *
 * 오브젝트 풀로 폰이 재사용되면 BT 가 다시 시작되면서 기준점도 새 스폰 위치로
 * 다시 잡힌다.
 */
UCLASS()
class ASCENDUNT_API UBTTask_FindRandomPatrolLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindRandomPatrolLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

protected:
	/** 계산 결과를 쓸 키. 뒤따르는 Move To 가 이 키를 읽는다. */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector PatrolLocationKey;

	/** 기준점에서 이 반경 안의 도달 가능한 지점을 고른다. */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "100.0"))
	float PatrolRadius = 1500.f;

	/**
	 * 직전 목적지에서 이 거리 안이면 다시 뽑는다.
	 * 없으면 제자리 근처만 왔다 갔다 해서 순찰처럼 보이지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float MinDistanceFromLast = 400.f;

	/** 조건을 만족하는 지점을 찾는 최대 시도 횟수. */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxAttempts = 4;

private:
	struct FPatrolMemory
	{
		FVector Origin = FVector::ZeroVector;
		FVector LastPicked = FVector::ZeroVector;
		bool bOriginSet = false;
	};
};

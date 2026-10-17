#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindCombatMoveLocation.generated.h"

/**
 * 교전 중 이동할 목적지를 계산해 블랙보드에 기록한다. 이동 자체는 하지 않는다.
 *
 * 실제 이동은 뒤이어 오는 엔진 기본 Move To 태스크가 맡는다. 경로 탐색, 도착 판정,
 * 중단 처리를 엔진에 맡길 수 있고 EQS 없이도 같은 조합(위치 산출 -> 이동)이 된다.
 *
 * ── 후진을 만들지 않는다 ────────────────────────────────────────────
 * 원거리 미니언의 로코모션에는 Jog_Fwd / Jog_Left / Jog_Right 만 있고 후진 애니가
 * 없다. 표적을 정면으로 본 채 뒤로 물러나면 재생할 애니가 없어 미끄러진다.
 *
 * 그래서 거리가 너무 가까울 때도 뒤로 빼지 않고 측면 이동량을 키워 크게 원을 그린다
 * (circle strafe). 결과적으로 좌우 애니만으로 전부 커버되고, 그림도 더 낫다.
 * 후진 애니가 생기면 bAllowRetreat 를 켜면 된다.
 */
UCLASS()
class ASCENDUNT_API UBTTask_FindCombatMoveLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindCombatMoveLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	/** 계산 결과를 쓸 키. 뒤따르는 Move To 가 이 키를 읽는다. */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector MoveLocationKey;

	/** 이 거리보다 가까우면 접근을 멈추고 측면 이동만 한다. */
	UPROPERTY(EditAnywhere, Category = "Combat|Range", meta = (ClampMin = "0.0"))
	float PreferredRangeMin = 800.f;

	/** 이 거리보다 멀면 접근 성분을 섞는다. */
	UPROPERTY(EditAnywhere, Category = "Combat|Range", meta = (ClampMin = "0.0"))
	float PreferredRangeMax = 1800.f;

	/** 한 번에 옆으로 이동할 거리. 너무 크면 춤추는 것처럼 보인다. */
	UPROPERTY(EditAnywhere, Category = "Combat|Strafe", meta = (ClampMin = "0.0"))
	float StrafeDistance = 400.f;

	/** 사거리보다 가까울 때 측면 이동량에 곱하는 배수. 크게 돌아 거리를 벌린다. */
	UPROPERTY(EditAnywhere, Category = "Combat|Strafe", meta = (ClampMin = "1.0"))
	float TooCloseStrafeScale = 1.5f;

	/** 좌우 방향을 유지하는 시간. 매번 뒤집으면 제자리에서 떠는 것처럼 보인다. */
	UPROPERTY(EditAnywhere, Category = "Combat|Strafe", meta = (ClampMin = "0.0"))
	float DirectionHoldTime = 2.5f;

	/**
	 * 후진 허용. 기본은 끔.
	 * 후진 애니메이션이 준비되기 전에 켜면 뒤로 미끄러지는 그림이 나온다.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Range")
	bool bAllowRetreat = false;

	/** 계산한 지점을 내비메시로 투영할 때 쓰는 탐색 범위. */
	UPROPERTY(EditAnywhere, Category = "Combat|Nav")
	FVector NavProjectionExtent = FVector(200.f, 200.f, 300.f);

	/**
	 * 계산 결과를 화면에 그린다.
	 * 초록 = 목적지 확정, 빨강 = 양쪽 다 내비메시 투영 실패(제자리 사격으로 폴백).
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Debug")
	bool bDrawDebugDestination = false;

private:
	struct FReposMemory
	{
		/** -1 = 좌, +1 = 우, 0 = 아직 정해지지 않음 */
		int8 StrafeSign = 0;

		/** 이 시각이 지나면 좌우를 다시 뽑는다. */
		float NextFlipTime = 0.f;
	};

	/** 좌우 방향을 결정한다. DirectionHoldTime 동안은 같은 방향을 유지한다. */
	int8 ResolveStrafeSign(FReposMemory& Memory, float WorldTime) const;
};

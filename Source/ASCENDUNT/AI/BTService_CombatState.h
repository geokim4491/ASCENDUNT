#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_CombatState.generated.h"

/**
 * 전투 판단에 필요한 값을 한 곳에서 계산해 블랙보드에 올린다.
 *
 * 전투 브랜치에 붙여 쓴다. 데코레이터마다 거리/시야를 각자 재계산하는 대신
 * 여기서 한 번만 구하고, 조건 분기는 전부 Blackboard Based Condition 으로 처리한다.
 * 트레이스가 틱당 1회로 줄고 판정 기준이 한 곳에 모인다.
 *
 * ── 기록하는 값 ─────────────────────────────────────────────────────
 *   DistanceToTarget  타깃까지의 거리. 타깃이 없으면 큰 값(MAX_FLT)을 넣어
 *                     "사거리 밖" 조건이 자연스럽게 참이 되게 한다.
 *   HasLineOfSight    시야 확보 여부.
 *   LastKnownLocation 시야가 확보된 동안 계속 갱신한다. 타깃을 놓치면 마지막
 *                     값이 남아 탐색 브랜치가 그곳으로 이동할 수 있다.
 *                     타깃이 사라져도 지우지 않는 것이 핵심이다.
 */
UCLASS()
class ASCENDUNT_API UBTService_CombatState : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_CombatState();

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector DistanceToTargetKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HasLineOfSightKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector LastKnownLocationKey;

	/**
	 * 시야 판정 트레이스 채널. 기본은 AscWeaponTrace(ECC_GameTraceChannel1).
	 *
	 * 사격용 채널을 그대로 쓰는 이유는 "보인다"와 "총알이 닿는다"를 일치시키기 위해서다.
	 * ECC_Visibility 는 프로젝트 콜리전 프로파일과 무관해서, 지오메트리가 그 채널을
	 * 막지 않으면 벽 뒤에 숨어도 시야가 확보된 것으로 나온다.
	 *
	 * AscWeaponTrace 는 DefaultResponse 가 Block 이라 월드가 막고,
	 * AscPawnCapsule 이 Ignore 라 아군 캡슐이 시야를 가리지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Line Of Sight")
	TEnumAsByte<ECollisionChannel> SightTraceChannel = ECC_GameTraceChannel1;

	/** 시야 트레이스를 화면에 그린다. 초록 = 확보, 빨강 = 차단. */
	UPROPERTY(EditAnywhere, Category = "Line Of Sight|Debug")
	bool bDrawDebugLineOfSight = false;

	/**
	 * 타깃 조준점의 높이 보정. 액터 위치는 발밑이 아니라 캡슐 중심이지만,
	 * 몸통을 노리도록 살짝 올려주면 바닥 턱에 시야가 막히는 오판이 줄어든다.
	 */
	UPROPERTY(EditAnywhere, Category = "Line Of Sight")
	float TargetAimHeightOffset = 0.f;

private:
	/** 눈높이에서 타깃까지 트레이스한다. 자신과 타깃은 무시한다. */
	bool TraceLineOfSight(const APawn& SelfPawn, const AActor& Target) const;
};

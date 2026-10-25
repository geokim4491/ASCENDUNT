#include "BTTask_FindRandomPatrolLocation.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"

UBTTask_FindRandomPatrolLocation::UBTTask_FindRandomPatrolLocation()
{
	NodeName = TEXT("Find Random Patrol Location");

	bCreateNodeInstance = false;

	PatrolLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindRandomPatrolLocation, PatrolLocationKey));
	PatrolLocationKey.SelectedKeyName = TEXT("PatrolLocation");
}

uint16 UBTTask_FindRandomPatrolLocation::GetInstanceMemorySize() const
{
	return sizeof(FPatrolMemory);
}

FString UBTTask_FindRandomPatrolLocation::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s\nRadius: %.0f\nKey: %s"),
		*Super::GetStaticDescription(),
		PatrolRadius,
		*PatrolLocationKey.SelectedKeyName.ToString());
}

EBTNodeResult::Type UBTTask_FindRandomPatrolLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FPatrolMemory* Memory = CastInstanceNodeMemory<FPatrolMemory>(NodeMemory);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* SelfPawn = AIC ? AIC->GetPawn() : nullptr;
	UWorld* World = SelfPawn ? SelfPawn->GetWorld() : nullptr;

	if (!BB || !SelfPawn || !World)
	{
		return EBTNodeResult::Failed;
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (!NavSys)
	{
		return EBTNodeResult::Failed;
	}

	// 기준점은 트리가 시작된 뒤 첫 실행에서 한 번만 잡는다.
	if (!Memory->bOriginSet)
	{
		Memory->Origin = SelfPawn->GetActorLocation();
		Memory->LastPicked = Memory->Origin;
		Memory->bOriginSet = true;
	}

	// 직전 목적지에서 충분히 떨어진 지점이 나올 때까지 몇 번 시도한다.
	// 다 실패하면 마지막 후보를 그대로 쓴다. 순찰이 멈추는 것보다는 낫다.
	FNavLocation Picked;
	bool bFound = false;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		FNavLocation Candidate;
		if (!NavSys->GetRandomReachablePointInRadius(Memory->Origin, PatrolRadius, Candidate))
		{
			continue;
		}

		Picked = Candidate;
		bFound = true;

		if (FVector::Dist(Candidate.Location, Memory->LastPicked) >= MinDistanceFromLast)
		{
			break;
		}
	}

	if (!bFound)
	{
		// 내비메시가 없거나 기준점이 내비메시 밖이다.
		return EBTNodeResult::Failed;
	}

	Memory->LastPicked = Picked.Location;
	BB->SetValueAsVector(PatrolLocationKey.SelectedKeyName, Picked.Location);

	return EBTNodeResult::Succeeded;
}

#include "BTTask_FindCombatMoveLocation.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"

UBTTask_FindCombatMoveLocation::UBTTask_FindCombatMoveLocation()
{
	NodeName = TEXT("Find Combat Move Location");

	bCreateNodeInstance = false;

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindCombatMoveLocation, TargetActorKey), AActor::StaticClass());
	MoveLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindCombatMoveLocation, MoveLocationKey));

	TargetActorKey.SelectedKeyName = TEXT("TargetActor");
	MoveLocationKey.SelectedKeyName = TEXT("TargetLocation");
}

uint16 UBTTask_FindCombatMoveLocation::GetInstanceMemorySize() const
{
	return sizeof(FReposMemory);
}

FString UBTTask_FindCombatMoveLocation::GetStaticDescription() const
{
	// 출력 키를 노드에 표시한다. 이 키와 뒤따르는 Move To 의 키가 어긋나면
	// 목적지는 정상 계산되는데 이동만 조용히 실패한다. 눈으로 바로 대조할 수 있게 한다.
	return FString::Printf(TEXT("%s\nWrite to: %s\nRange: %.0f ~ %.0f\nStrafe: %.0f%s"),
		*Super::GetStaticDescription(),
		*MoveLocationKey.SelectedKeyName.ToString(),
		PreferredRangeMin, PreferredRangeMax, StrafeDistance,
		bAllowRetreat ? TEXT(" (후진 허용)") : TEXT(""));
}

int8 UBTTask_FindCombatMoveLocation::ResolveStrafeSign(FReposMemory& Memory, float WorldTime) const
{
	if (Memory.StrafeSign == 0 || WorldTime >= Memory.NextFlipTime)
	{
		Memory.StrafeSign = FMath::RandBool() ? 1 : -1;

		// 유지 시간에 편차를 준다. 여러 마리가 동시에 방향을 바꾸면 대열처럼 보인다.
		Memory.NextFlipTime = WorldTime + DirectionHoldTime * FMath::FRandRange(0.7f, 1.3f);
	}

	return Memory.StrafeSign;
}

EBTNodeResult::Type UBTTask_FindCombatMoveLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FReposMemory* Memory = CastInstanceNodeMemory<FReposMemory>(NodeMemory);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* SelfPawn = AIC ? AIC->GetPawn() : nullptr;
	UWorld* World = SelfPawn ? SelfPawn->GetWorld() : nullptr;

	if (!BB || !SelfPawn || !World)
	{
		return EBTNodeResult::Failed;
	}

	const AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!Target)
	{
		return EBTNodeResult::Failed;
	}

	const FVector SelfLoc = SelfPawn->GetActorLocation();

	// 수평 성분만 쓴다. 높이차가 있으면 방향이 기울어 목적지가 공중이나 지하로 간다.
	FVector ToTarget = Target->GetActorLocation() - SelfLoc;
	ToTarget.Z = 0.f;

	const float Distance = ToTarget.Size();
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return EBTNodeResult::Failed;
	}

	const FVector Forward = ToTarget / Distance;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);

	const int8 Sign = ResolveStrafeSign(*Memory, World->GetTimeSeconds());

	// ---- 측면 성분 ----
	float LateralAmount = StrafeDistance;

	// ---- 전후 성분 ----
	float RadialAmount = 0.f;

	if (Distance > PreferredRangeMax)
	{
		// 너무 멀다. 접근한다. 한 번에 다 좁히지 않고 StrafeDistance 만큼만 당긴다.
		RadialAmount = FMath::Min(Distance - PreferredRangeMax, StrafeDistance);
	}
	else if (Distance < PreferredRangeMin)
	{
		if (bAllowRetreat)
		{
			RadialAmount = -FMath::Min(PreferredRangeMin - Distance, StrafeDistance);
		}
		else
		{
			// 후진 애니가 없다. 뒤로 빼는 대신 크게 돌아서 거리를 번다.
			LateralAmount *= TooCloseStrafeScale;
		}
	}

	const FVector Desired = SelfLoc + (Right * Sign * LateralAmount) + (Forward * RadialAmount);

	// ---- 내비메시 투영 ----
	// 투영하지 않으면 벽 안이나 낭떠러지로 목적지가 잡혀 MoveTo 가 즉시 실패한다.
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (!NavSys)
	{
		return EBTNodeResult::Failed;
	}

	FNavLocation Projected;
	if (!NavSys->ProjectPointToNavigation(Desired, Projected, NavProjectionExtent))
	{
		// 그 방향이 막혔다. 반대편을 한 번 시도한다.
		Memory->StrafeSign = static_cast<int8>(-Sign);
		Memory->NextFlipTime = World->GetTimeSeconds() + DirectionHoldTime;

		const FVector Mirrored = SelfLoc + (Right * Memory->StrafeSign * LateralAmount) + (Forward * RadialAmount);
		if (!NavSys->ProjectPointToNavigation(Mirrored, Projected, NavProjectionExtent))
		{
			// 양쪽 다 막혔다. 제자리에서 사격을 계속하도록 실패를 돌려준다.
			if (bDrawDebugDestination)
			{
				DrawDebugLine(World, SelfLoc, Desired, FColor::Red, false, 1.f, 0, 2.f);
				DrawDebugSphere(World, Desired, 40.f, 8, FColor::Red, false, 1.f);
			}

			return EBTNodeResult::Failed;
		}
	}

	BB->SetValueAsVector(MoveLocationKey.SelectedKeyName, Projected.Location);

	if (bDrawDebugDestination)
	{
		DrawDebugLine(World, SelfLoc, Projected.Location, FColor::Green, false, 1.f, 0, 2.f);
		DrawDebugSphere(World, Projected.Location, 40.f, 8, FColor::Green, false, 1.f);
	}

	return EBTNodeResult::Succeeded;
}

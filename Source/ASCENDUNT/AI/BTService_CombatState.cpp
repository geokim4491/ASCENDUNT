#include "BTService_CombatState.h"

#include "AIController.h"
#include "ASCENDUNT/AI/BaseAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UBTService_CombatState::UBTService_CombatState()
{
	NodeName = TEXT("Combat State");

	bNotifyTick = true;
	bNotifyBecomeRelevant = true;
	bCreateNodeInstance = false;

	// 반응성과 비용의 타협점. 트레이스가 초당 5회로 제한된다.
	Interval = 0.2f;
	RandomDeviation = 0.05f;

	// 에디터에서 타입이 맞는 키만 고를 수 있게 필터를 건다.
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_CombatState, TargetActorKey), AActor::StaticClass());
	DistanceToTargetKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_CombatState, DistanceToTargetKey));
	HasLineOfSightKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_CombatState, HasLineOfSightKey));
	LastKnownLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_CombatState, LastKnownLocationKey));

	// 프로젝트 표준 키 이름을 기본값으로 넣어둔다.
	TargetActorKey.SelectedKeyName = TEXT("TargetActor");
	DistanceToTargetKey.SelectedKeyName = TEXT("DistanceToTarget");
	HasLineOfSightKey.SelectedKeyName = TEXT("HasLineOfSight");
	LastKnownLocationKey.SelectedKeyName = TEXT("LastKnownLocation");
}

FString UBTService_CombatState::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s\nDistance / LineOfSight / LastKnownLocation 갱신"),
		*Super::GetStaticDescription());
}

void UBTService_CombatState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* SelfPawn = AIC ? AIC->GetPawn() : nullptr;

	if (!BB || !SelfPawn)
	{
		return;
	}

	const AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));

	if (Target)
	{
		if (ABaseAIController* AscAI = Cast<ABaseAIController>(AIC))
		{
			if (!AscAI->IsValidCombatTarget(Target))
			{
				AscAI->ClearTargetActor();
				Target = nullptr;
			}
		}
	}

	if (!Target)
	{
		// 타깃이 없으면 "멀고 안 보인다"로 둔다.
		// 거리에 0을 넣으면 "사거리 안" 조건이 참이 되어 엉뚱한 브랜치가 열린다.
		BB->SetValueAsFloat(DistanceToTargetKey.SelectedKeyName, MAX_FLT);
		BB->SetValueAsBool(HasLineOfSightKey.SelectedKeyName, false);

		// LastKnownLocation 은 일부러 건드리지 않는다.
		// 마지막으로 본 위치가 남아 있어야 탐색 브랜치가 그곳으로 갈 수 있다.
		return;
	}

	const float Distance = FVector::Dist(SelfPawn->GetActorLocation(), Target->GetActorLocation());
	BB->SetValueAsFloat(DistanceToTargetKey.SelectedKeyName, Distance);

	const bool bHasLOS = TraceLineOfSight(*SelfPawn, *Target);
	BB->SetValueAsBool(HasLineOfSightKey.SelectedKeyName, bHasLOS);

	if (bHasLOS)
	{
		BB->SetValueAsVector(LastKnownLocationKey.SelectedKeyName, Target->GetActorLocation());
	}
}

bool UBTService_CombatState::TraceLineOfSight(const APawn& SelfPawn, const AActor& Target) const
{
	const UWorld* World = SelfPawn.GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Start = SelfPawn.GetPawnViewLocation();
	const FVector End = Target.GetActorLocation() + FVector(0.f, 0.f, TargetAimHeightOffset);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AscAICombatLOS), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(&SelfPawn);
	Params.AddIgnoredActor(&Target);

	// 자기 자신이 들고 있는 무기도 시야를 막으면 안 된다.
	TArray<AActor*> Attached;
	SelfPawn.GetAttachedActors(Attached);
	Params.AddIgnoredActors(Attached);

	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, Start, End, SightTraceChannel, Params);

	if (bDrawDebugLineOfSight)
	{
		// Interval 보다 살짝 길게 그려 깜빡임을 없앤다.
		const float DrawTime = Interval * 1.5f;

		DrawDebugLine(World, Start, bBlocked ? Hit.ImpactPoint : End,
			bBlocked ? FColor::Red : FColor::Green, false, DrawTime, 0, 1.5f);

		if (bBlocked)
		{
			DrawDebugPoint(World, Hit.ImpactPoint, 10.f, FColor::Red, false, DrawTime);
		}
	}

	return !bBlocked;
}

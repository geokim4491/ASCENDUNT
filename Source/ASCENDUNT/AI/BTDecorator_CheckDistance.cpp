#include "BTDecorator_CheckDistance.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTDecorator_CheckDistance::UBTDecorator_CheckDistance()
{
	NodeName = "Check Target Distance";
}

bool UBTDecorator_CheckDistance::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!AIC || !AIC->GetPawn()) return false;

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return false;

	// 블랙보드에서 타겟 액터 가져오기
	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Target) return false;

	// 나와 타겟 사이의 거리 계산
	float Distance = FVector::Dist(AIC->GetPawn()->GetActorLocation(), Target->GetActorLocation());

	// 사거리 안으로 들어왔으면 True 반환
	return Distance <= AttackRange;
}

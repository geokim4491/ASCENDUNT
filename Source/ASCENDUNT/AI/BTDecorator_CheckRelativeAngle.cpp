#include "BTDecorator_CheckRelativeAngle.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Actor.h"

UBTDecorator_CheckRelativeAngle::UBTDecorator_CheckRelativeAngle()
{
	NodeName = "Check Relative Angle";
	MinAngle = -45.0f;
	MaxAngle = 45.0f;
}

bool UBTDecorator_CheckRelativeAngle::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!AIC) return false;

	APawn* AIPawn = AIC->GetPawn();
	if (!AIPawn) return false;

	UBlackboardComponent* BBComp = OwnerComp.GetBlackboardComponent();
	if (!BBComp) return false;

	AActor* TargetActor = Cast<AActor>(BBComp->GetValueAsObject(BlackboardKey.SelectedKeyName));
	if (!TargetActor) return false;

	FVector DirectionToTarget = TargetActor->GetActorLocation() - AIPawn->GetActorLocation();
	DirectionToTarget.Z = 0.0f; // 높이 차이 무시
	
	if (DirectionToTarget.IsNearlyZero())
	{
		return true; // 겹쳐있으면 무조건 통과 (또는 false 처리 가능)
	}
	DirectionToTarget.Normalize();

	FVector ForwardVector = AIPawn->GetActorForwardVector();
	ForwardVector.Z = 0.0f;
	ForwardVector.Normalize();

	FVector RightVector = AIPawn->GetActorRightVector();
	RightVector.Z = 0.0f;
	RightVector.Normalize();

	float DotForward = FVector::DotProduct(ForwardVector, DirectionToTarget);
	float DotRight = FVector::DotProduct(RightVector, DirectionToTarget);

	// 각도 계산 (-180 ~ 180도)
	float Angle = FMath::RadiansToDegrees(FMath::Atan2(DotRight, DotForward));

	// Wrap-around 체크 (예: 뒤쪽을 체크하기 위해 Min: 135, Max: -135 인 경우)
	if (MinAngle > MaxAngle)
	{
		return Angle >= MinAngle || Angle <= MaxAngle;
	}
	else
	{
		return Angle >= MinAngle && Angle <= MaxAngle;
	}
}

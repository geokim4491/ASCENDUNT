#include "BTDecorator_CheckGameplayTag.h"
#include "AIController.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"

UBTDecorator_CheckGameplayTag::UBTDecorator_CheckGameplayTag()
{
	NodeName = "Check Gameplay Tag";
	bRequireTag = true;
	bNotifyTick = true;
	
	// Observer Aborts 드롭다운을 에디터에 표시하기 위한 설정
	bAllowAbortNone = true;
	bAllowAbortLowerPri = true;
	bAllowAbortChildNodes = true;
}

bool UBTDecorator_CheckGameplayTag::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!AIC) return false;

	APawn* AIPawn = AIC->GetPawn();
	if (!AIPawn) return false;

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(AIPawn);
	if (!ASI) return false;

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (!ASC) return false;

	bool bHasTag = ASC->HasMatchingGameplayTag(TagToCheck);

	return bRequireTag ? bHasTag : !bHasTag;
}

void UBTDecorator_CheckGameplayTag::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	// 상태가 변했는지 틱마다 확인하여 트리 중단(Abort) 처리
	bool bConditionMet = CalculateRawConditionValue(OwnerComp, NodeMemory);
	
	// 원래 조건과 달라졌고, Observer가 설정되어 있다면 Abort 시킴
	// 간단한 실시간 감지를 위해 기본 데코레이터 기능을 재평가하도록 유도
	OwnerComp.RequestExecution(this);
}

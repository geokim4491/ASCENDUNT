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

uint16 UBTDecorator_CheckGameplayTag::GetInstanceMemorySize() const
{
	return sizeof(FTagDecoratorMemory);
}

void UBTDecorator_CheckGameplayTag::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	// 관찰을 시작하는 시점의 조건을 기준값으로 잡아 둔다.
	// 이게 없으면 첫 틱이 항상 "변했다"로 판정되어 불필요한 재평가가 한 번 발생한다.
	FTagDecoratorMemory* Memory = CastInstanceNodeMemory<FTagDecoratorMemory>(NodeMemory);
	Memory->bLastResult = CalculateRawConditionValue(OwnerComp, NodeMemory);
}

void UBTDecorator_CheckGameplayTag::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	// 게임플레이 태그는 BT 외부에서 바뀌므로 여기서 폴링해 변화를 감지한다.
	//
	// 단, 재평가 요청은 "값이 실제로 바뀐 순간"에만 해야 한다.
	// 매 틱 무조건 RequestExecution 을 부르면 실행 중인 브랜치가 매 프레임 중단/재시작되어
	// MoveTo 가 가속과 정지를 반복하고, 몽타주가 재생 도중 다시 시작되며,
	// 어빌리티가 끝나자마자 재활성화되는 증상이 한꺼번에 나타난다.
	FTagDecoratorMemory* Memory = CastInstanceNodeMemory<FTagDecoratorMemory>(NodeMemory);
	const bool bConditionMet = CalculateRawConditionValue(OwnerComp, NodeMemory);

	if (Memory->bLastResult == bConditionMet)
	{
		return;
	}

	Memory->bLastResult = bConditionMet;
	OwnerComp.RequestExecution(this);
}

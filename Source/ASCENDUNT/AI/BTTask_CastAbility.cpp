#include "BTTask_CastAbility.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

UBTTask_CastAbility::UBTTask_CastAbility() { NodeName = "Cast GAS Ability"; }

EBTNodeResult::Type UBTTask_CastAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC || !AIC->GetPawn()) return EBTNodeResult::Failed;

    // 캐릭터의 ASC를 가져옴
    UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AIC->GetPawn());
    if (!ASC) return EBTNodeResult::Failed;

    // 태그를 통해 스킬(GA) 발동 시도
    FGameplayTagContainer TagContainer(AbilityTag);
    bool bSuccess = ASC->TryActivateAbilitiesByTag(TagContainer);

    // 스킬 발동에 성공하면 Task 성공, 아니면 실패 처리
    return bSuccess ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
